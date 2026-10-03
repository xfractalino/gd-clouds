#include "volumetric_clouds_effect.h"

#include <cstring>

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/rd_sampler_state.hpp>
#include <godot_cpp/classes/rd_shader_file.hpp>
#include <godot_cpp/classes/rd_shader_source.hpp>
#include <godot_cpp/classes/rd_shader_spirv.hpp>
#include <godot_cpp/classes/render_scene_data.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/uniform_set_cache_rd.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/projection.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "rd_utils.h"
#include "shader_builder.h"

namespace {

const char *const SHADER_MODULE_DIR = "res://addons/gd_clouds/shaders/lib/";
const char *const UPSAMPLER_SHADER_PATH = "res://addons/gd_clouds/shaders/upsampler.glsl";
const char *const GENERATED_SOURCE_DUMP_PATH = "user://clouds_march_generated.glsl";

// The compute shaders work on tiles of this many pixels per side.
constexpr uint32_t GROUP_SIZE = 8;

inline uint32_t group_count(int p_pixels) {
	return (static_cast<uint32_t>(p_pixels) + GROUP_SIZE - 1) / GROUP_SIZE;
}

String build_ray_marcher_source() {
	const auto path = [](const char *p_file) {
		return String(SHADER_MODULE_DIR) + String(p_file);
	};

	ShaderBuilder builder;

	builder.add_module("cloud_interface", path("clouds_compute_interface.gdshaderinc"));
	builder.add_module("math", path("math.gdshaderinc"));
	builder.add_module("phase", path("phase.gdshaderinc"), { "math", "cloud_interface" });
	builder.add_module("cloud_density", path("cloud_density.gdshaderinc"), { "math", "cloud_interface" });
	builder.add_module("cloud_march", path("cloud_march.gdshaderinc"), { "math", "phase", "cloud_density", "cloud_interface" });
	builder.add_module("cloud_main", path("clouds_compute_main.gdshaderinc"), { "math", "cloud_march", "cloud_interface" });

	// The interface module has to come first as it carries the version directive.
	return builder.build({ "cloud_interface", "cloud_main" });
}

bool dump_generated_source(const String &p_source) {
	const Ref<FileAccess> file = FileAccess::open(GENERATED_SOURCE_DUMP_PATH, FileAccess::WRITE);
	ERR_FAIL_COND_V_MSG(file.is_null(), false, vformat("Cannot write generated shader source to %s", GENERATED_SOURCE_DUMP_PATH));

	file->store_string(p_source);

	return true;
}

Ref<RDShaderSPIRV> compile_ray_marcher(RenderingDevice *p_rendering_device) {
	const String source = build_ray_marcher_source();

	if (source.is_empty()) {
		return {};
	}

	Ref<RDShaderSource> shader_source;
	shader_source.instantiate();
	shader_source->set_language(RenderingDevice::SHADER_LANGUAGE_GLSL);
	shader_source->set_stage_source(RenderingDevice::SHADER_STAGE_COMPUTE, source);

	const Ref<RDShaderSPIRV> spirv = p_rendering_device->shader_compile_spirv_from_source(shader_source);
	ERR_FAIL_COND_V_MSG(spirv.is_null(), Ref<RDShaderSPIRV>(), "Failed to compile the generated cloud shader.");

	const String error = spirv->get_stage_compile_error(RenderingDevice::SHADER_STAGE_COMPUTE);

	if (!error.is_empty()) {
		String message = "Error in generated cloud shader: " + error;

		if (dump_generated_source(source)) {
			message += vformat("\nThe generated source has been written to %s", GENERATED_SOURCE_DUMP_PATH);
		}

		ERR_FAIL_V_MSG(Ref<RDShaderSPIRV>(), message);
	}

	return spirv;
}

Ref<RDShaderSPIRV> load_spirv(const String &p_path) {
	const Ref<RDShaderFile> shader_file = ResourceLoader::get_singleton()->load(p_path, "RDShaderFile", ResourceLoader::CACHE_MODE_IGNORE);
	ERR_FAIL_COND_V_MSG(shader_file.is_null(), Ref<RDShaderSPIRV>(), vformat("Failed to load shader file: %s", p_path));

	const Ref<RDShaderSPIRV> spirv = shader_file->get_spirv();
	ERR_FAIL_COND_V_MSG(spirv.is_null(), Ref<RDShaderSPIRV>(), vformat("Shader file has no SPIR-V: %s", p_path));

	const String error = spirv->get_stage_compile_error(RenderingDevice::SHADER_STAGE_COMPUTE);
	ERR_FAIL_COND_V_MSG(!error.is_empty(), Ref<RDShaderSPIRV>(), vformat("Error in shader %s: %s", p_path, error));

	return spirv;
}

} // namespace

VolumetricCloudsEffect::VolumetricCloudsEffect() {
	set_effect_callback_type(EFFECT_CALLBACK_TYPE_POST_TRANSPARENT);
	set_access_resolved_color(true);
	set_access_resolved_depth(true);
}

VolumetricCloudsEffect::~VolumetricCloudsEffect() {
	if (rendering_device == nullptr) {
		return;
	}

	RenderingServer *rendering_server = RenderingServer::get_singleton();

	if (rendering_server == nullptr) {
		return;
	}

	// The RIDs have to be freed on the render thread, and by the time that gets to run this object is gone. So they are
	// handed over by value.
	rendering_server->call_on_render_thread(callable_mp_static(&VolumetricCloudsEffect::_free_rids).bind(_get_owned_rids()));
}

void VolumetricCloudsEffect::_render_callback(int32_t p_effect_callback_type, RenderData *p_render_data) {
	// Temporarily disable cloud rendering in editor
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	if (p_effect_callback_type != EFFECT_CALLBACK_TYPE_POST_TRANSPARENT || p_render_data == nullptr) {
		return;
	}

	if (pipeline_state == PIPELINE_FAILED) {
		return;
	}

	if (noise_texture.is_null()) {
		if (!missing_noise_reported) {
			missing_noise_reported = true;
			ERR_PRINT("VolumetricCloudsEffect has no noise texture, so the clouds can't be rendered. Bake one with CloudNoise.bake() and assign it to noise_texture.");
		}

		return;
	}

	// This comes after the checks above, so that an instance that never renders anything doesn't load the shaders and
	// build the compute pipelines.
	if (pipeline_state == PIPELINE_UNINITIALIZED && !_initialize_pipeline()) {
		return;
	}

	const Ref<RenderSceneBuffersRD> scene_buffers = p_render_data->get_render_scene_buffers();
	const RenderSceneData *scene_data = p_render_data->get_render_scene_data();

	if (scene_buffers.is_null() || scene_data == nullptr) {
		return;
	}

	const Vector2i size = scene_buffers->get_internal_size();

	if (size.x == 0 || size.y == 0) {
		return;
	}

	const RID noise = RenderingServer::get_singleton()->texture_get_rd_texture(noise_texture->get_rid());

	if (!noise.is_valid()) {
		return;
	}

	const CloudsConfig &config = clouds_config.is_valid() ? **clouds_config : **default_config;

	const int divisor = MAX(config.get_resolution_divisor(), 1);
	const Vector2i march_size(MAX((size.x + divisor - 1) / divisor, 1), MAX((size.y + divisor - 1) / divisor, 1));

	const uint32_t view_count = scene_buffers->get_view_count();

	_ensure_cloud_texture(scene_buffers, march_size, view_count);

	RenderingDevice *rd = rendering_device;

	for (uint32_t view = 0; view < view_count; ++view) {
		const RID color = scene_buffers->get_texture_slice(render_buffers_context, color_texture_name, view, 0, 1, 1);
		const RID depth = scene_buffers->get_texture_slice(render_buffers_context, depth_texture_name, view, 0, 1, 1);
		const RID cloud_texture = scene_buffers->get_texture_slice(cloud_context, cloud_texture_name, view, 0, 1, 1);

		if (!cloud_texture.is_valid()) {
			continue;
		}

		const Projection projection = scene_data->get_view_projection(view);
		const Vector3 eye_offset = scene_data->get_view_eye_offset(view);
		const Transform3D view_transform = scene_data->get_cam_transform().translated_local(eye_offset);

		float matrices[PUSH_CONSTANTS_SIZE / sizeof(float)];
		rd_utils::write_mat4(rd_utils::write_mat4(matrices, projection.inverse()), Projection(view_transform));
		memcpy(push_constants.ptrw(), matrices, PUSH_CONSTANTS_SIZE);

		_update_params(config, size, march_size, view_transform.origin);
		rd->buffer_update(params_buffer, 0, PARAMS_BUFFER_SIZE, params);

		// Pass 1: ray marcher.
		rd_utils::update_image(march_target_uniform, 0, cloud_texture);
		rd_utils::update_sampled(march_depth_uniform, 1, depth_sampler, depth);
		rd_utils::update_sampled(march_noise_uniform, 2, noise_sampler, noise);
		rd_utils::update_uniform_buffer(march_params_uniform, 3, params_buffer);

		const RID march_set = UniformSetCacheRD::get_cache(ray_marcher_shader, 0, march_uniforms);

		// Pass 2: bilateral upsample and composite.
		rd_utils::update_image(upsample_color_uniform, 0, color);
		rd_utils::update_sampled(upsample_depth_uniform, 1, depth_sampler, depth);
		rd_utils::update_sampled(upsample_cloud_uniform, 2, depth_sampler, cloud_texture);
		rd_utils::update_uniform_buffer(upsample_params_uniform, 3, params_buffer);

		const RID upsample_set = UniformSetCacheRD::get_cache(upsampler_shader, 0, upsample_uniforms);

		if (!march_set.is_valid() || !upsample_set.is_valid()) {
			continue;
		}

		if (profile_gpu) {
			rd->capture_timestamp("clouds_march_begin");
		}

		int64_t list = rd->compute_list_begin();
		rd->compute_list_bind_compute_pipeline(list, ray_marcher_pipeline);
		rd->compute_list_bind_uniform_set(list, march_set, 0);
		rd->compute_list_set_push_constant(list, push_constants, PUSH_CONSTANTS_SIZE);
		rd->compute_list_dispatch(list, group_count(march_size.x), group_count(march_size.y), 1);
		rd->compute_list_end();

		if (profile_gpu) {
			rd->capture_timestamp("clouds_upsample_begin");
		}

		list = rd->compute_list_begin();
		rd->compute_list_bind_compute_pipeline(list, upsampler_pipeline);
		rd->compute_list_bind_uniform_set(list, upsample_set, 0);
		rd->compute_list_set_push_constant(list, push_constants, PUSH_CONSTANTS_SIZE);
		rd->compute_list_dispatch(list, group_count(size.x), group_count(size.y), 1);
		rd->compute_list_end();

		if (profile_gpu) {
			rd->capture_timestamp("clouds_end");
		}
	}
}

bool VolumetricCloudsEffect::_initialize_pipeline() {
	// Anything that goes wrong below leaves it like this.
	pipeline_state = PIPELINE_FAILED;

	RenderingDevice *rd = RenderingServer::get_singleton()->get_rendering_device();

	if (rd == nullptr) {
		return false;
	}

	const Ref<RDShaderSPIRV> ray_marcher_spirv = compile_ray_marcher(rd);
	const Ref<RDShaderSPIRV> upsampler_spirv = load_spirv(UPSAMPLER_SHADER_PATH);

	if (ray_marcher_spirv.is_null() || upsampler_spirv.is_null()) {
		return false;
	}

	rendering_device = rd;

	ray_marcher_shader = rd->shader_create_from_spirv(ray_marcher_spirv, "clouds_ray_marcher");
	upsampler_shader = rd->shader_create_from_spirv(upsampler_spirv, "clouds_upsampler");

	if (ray_marcher_shader.is_valid()) {
		ray_marcher_pipeline = rd->compute_pipeline_create(ray_marcher_shader);
	}

	if (upsampler_shader.is_valid()) {
		upsampler_pipeline = rd->compute_pipeline_create(upsampler_shader);
	}

	Ref<RDSamplerState> depth_sampler_state;
	depth_sampler_state.instantiate();
	depth_sampler_state->set_mag_filter(RenderingDevice::SAMPLER_FILTER_NEAREST);
	depth_sampler_state->set_min_filter(RenderingDevice::SAMPLER_FILTER_NEAREST);
	depth_sampler_state->set_repeat_u(RenderingDevice::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE);
	depth_sampler_state->set_repeat_v(RenderingDevice::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE);
	depth_sampler = rd->sampler_create(depth_sampler_state);

	Ref<RDSamplerState> noise_sampler_state;
	noise_sampler_state.instantiate();
	noise_sampler_state->set_mag_filter(RenderingDevice::SAMPLER_FILTER_LINEAR);
	noise_sampler_state->set_min_filter(RenderingDevice::SAMPLER_FILTER_LINEAR);
	noise_sampler_state->set_repeat_u(RenderingDevice::SAMPLER_REPEAT_MODE_REPEAT);
	noise_sampler_state->set_repeat_v(RenderingDevice::SAMPLER_REPEAT_MODE_REPEAT);
	noise_sampler_state->set_repeat_w(RenderingDevice::SAMPLER_REPEAT_MODE_REPEAT);
	noise_sampler = rd->sampler_create(noise_sampler_state);

	push_constants.resize(PUSH_CONSTANTS_SIZE);
	params.resize(PARAMS_BUFFER_SIZE);
	params.fill(0);
	params_buffer = rd->uniform_buffer_create(PARAMS_BUFFER_SIZE, params);

	if (!ray_marcher_pipeline.is_valid() || !upsampler_pipeline.is_valid() || !depth_sampler.is_valid() || !noise_sampler.is_valid() || !params_buffer.is_valid()) {
		_free_pipeline();
		pipeline_state = PIPELINE_FAILED;

		ERR_FAIL_V_MSG(false, "Failed to create the compute pipelines of the clouds.");
	}

	march_uniforms.clear();
	upsample_uniforms.clear();

	for (Ref<RDUniform> *uniform : { &march_target_uniform, &march_depth_uniform, &march_noise_uniform, &march_params_uniform }) {
		uniform->instantiate();
		march_uniforms.push_back(*uniform);
	}

	for (Ref<RDUniform> *uniform : { &upsample_color_uniform, &upsample_depth_uniform, &upsample_cloud_uniform, &upsample_params_uniform }) {
		uniform->instantiate();
		upsample_uniforms.push_back(*uniform);
	}

	if (default_config.is_null()) {
		default_config.instantiate();
	}

	pipeline_state = PIPELINE_READY;

	return true;
}

void VolumetricCloudsEffect::_free_pipeline() {
	if (rendering_device != nullptr) {
		_free_rids(_get_owned_rids());
	}

	rendering_device = nullptr;

	ray_marcher_shader = RID();
	ray_marcher_pipeline = RID();
	upsampler_shader = RID();
	upsampler_pipeline = RID();
	depth_sampler = RID();
	noise_sampler = RID();
	params_buffer = RID();

	pipeline_state = PIPELINE_UNINITIALIZED;

	cloud_texture_size = Vector2i();
	cloud_texture_views = 0;
}

void VolumetricCloudsEffect::_free_rids(const Array &p_rids) {
	RenderingServer *rendering_server = RenderingServer::get_singleton();

	if (rendering_server == nullptr) {
		return;
	}

	RenderingDevice *rd = rendering_server->get_rendering_device();

	if (rd == nullptr) {
		return;
	}

	for (int64_t i = 0; i < p_rids.size(); ++i) {
		const RID rid = p_rids[i];

		if (rid.is_valid()) {
			rd->free_rid(rid);
		}
	}
}

Array VolumetricCloudsEffect::_get_owned_rids() const {
	// The pipelines are left out, as freeing a shader also frees what depends on it.
	Array rids;
	rids.push_back(ray_marcher_shader);
	rids.push_back(upsampler_shader);
	rids.push_back(depth_sampler);
	rids.push_back(noise_sampler);
	rids.push_back(params_buffer);

	return rids;
}

void VolumetricCloudsEffect::_ensure_cloud_texture(const Ref<RenderSceneBuffersRD> &p_scene_buffers, const Vector2i &p_march_size, uint32_t p_view_count) {
	const bool exists = p_scene_buffers->has_texture(cloud_context, cloud_texture_name);

	if (exists && cloud_texture_size == p_march_size && cloud_texture_views == p_view_count) {
		return;
	}

	if (exists) {
		p_scene_buffers->clear_context(cloud_context);
	}

	const uint32_t usage = RenderingDevice::TEXTURE_USAGE_STORAGE_BIT | RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT;

	p_scene_buffers->create_texture(cloud_context, cloud_texture_name, RenderingDevice::DATA_FORMAT_R16G16B16A16_SFLOAT, usage, RenderingDevice::TEXTURE_SAMPLES_1, p_march_size, p_view_count, 1, true, false);

	cloud_texture_size = p_march_size;
	cloud_texture_views = p_view_count;
}

void VolumetricCloudsEffect::_update_params(const CloudsConfig &p_config, const Vector2i &p_full_size, const Vector2i &p_march_size, const Vector3 &p_camera_position) {
	const float inner_height = p_config.get_cloud_inner_height();
	float outer_height = p_config.get_cloud_outer_height();

	// The config can't check this by itself, see the comment on CloudsConfig. Rendering isn't rejected over it, as the
	// shader still works with the layer clamped to a minimal thickness.
	if (!(outer_height > inner_height)) {
		outer_height = inner_height + 1.0f;

		if (Math::abs(inner_height - last_checked_inner_height) > 0.001f || Math::abs(p_config.get_cloud_outer_height() - last_checked_outer_height) > 0.001f) {
			ERR_PRINT(vformat("VolumetricCloudsEffect: cloud_outer_height (%f) must be greater than cloud_inner_height (%f), otherwise the cloud layer has no thickness. Clamping it to %f.", p_config.get_cloud_outer_height(), inner_height, outer_height));
		}
	}

	last_checked_inner_height = inner_height;
	last_checked_outer_height = p_config.get_cloud_outer_height();

	const Vector3 planet_center = p_config.get_planet_center();
	const float planet_radius = p_config.get_planet_radius();
	const Vector3 sun = sun_direction.normalized();

	const auto full_width = static_cast<float>(p_full_size.x);
	const auto full_height = static_cast<float>(p_full_size.y);
	const auto march_width = static_cast<float>(p_march_size.x);
	const auto march_height = static_cast<float>(p_march_size.y);

	// This has to match the Params block of the shaders.
	float values[PARAMS_BUFFER_SIZE / sizeof(float)] = {};
	float *value = values;

	value = rd_utils::write_vec4(value, static_cast<float>(planet_center.x), static_cast<float>(planet_center.y), static_cast<float>(planet_center.z), 0.0f);
	value = rd_utils::write_vec4(value, planet_radius + inner_height, planet_radius + outer_height, p_config.get_cloud_tile_size(), p_config.get_density_multiplier());
	value = rd_utils::write_vec4(value, full_width, full_height, 1.0f / full_width, 1.0f / full_height);
	value = rd_utils::write_vec4(value, march_width, march_height, 1.0f / march_width, 1.0f / march_height);
	value = rd_utils::write_vec4(value, static_cast<float>(p_camera_position.x), static_cast<float>(p_camera_position.y), static_cast<float>(p_camera_position.z), 0.0f);
	value = rd_utils::write_vec4(value, static_cast<float>(sun.x), static_cast<float>(sun.y), static_cast<float>(sun.z), sun_energy);
	rd_utils::write_vec4(value, static_cast<float>(p_config.get_march_steps()), static_cast<float>(p_config.get_light_steps()), p_config.get_max_march_distance(), p_config.get_coverage());

	memcpy(params.ptrw(), values, PARAMS_BUFFER_SIZE);
}

void VolumetricCloudsEffect::_report_timestamps() {
	if (rendering_device == nullptr) {
		return;
	}

	const uint32_t count = rendering_device->get_captured_timestamps_count();
	uint64_t previous_time = 0;
	String previous_name;

	for (uint32_t i = 0; i < count; ++i) {
		const String name = rendering_device->get_captured_timestamp_name(i);
		const uint64_t gpu_time = rendering_device->get_captured_timestamp_gpu_time(i);

		// Print only the timestamps of the clouds, and don't print clouds_end to clouds_march_begin when there are
		// multiple views.
		if (previous_name.begins_with("clouds_") && !name.begins_with("clouds_march_begin")) {
			UtilityFunctions::print(previous_name, " -> ", name, ": ", gpu_time - previous_time);
		}

		previous_time = gpu_time;
		previous_name = name;
	}
}

void VolumetricCloudsEffect::set_clouds_config(const Ref<CloudsConfig> &p_clouds_config) {
	clouds_config = p_clouds_config;
}

Ref<CloudsConfig> VolumetricCloudsEffect::get_clouds_config() const {
	return clouds_config;
}

void VolumetricCloudsEffect::set_noise_texture(const Ref<Texture3D> &p_noise_texture) {
	noise_texture = p_noise_texture;
	missing_noise_reported = false;
}

Ref<Texture3D> VolumetricCloudsEffect::get_noise_texture() const {
	return noise_texture;
}

void VolumetricCloudsEffect::set_sun_direction(const Vector3 &p_sun_direction) {
	// A zero direction leaves the sun with no direction to shine from. It doesn't need to be unit length though, as it
	// gets normalised before being passed to the shaders.
	ERR_FAIL_COND_MSG(!p_sun_direction.is_finite() || p_sun_direction.is_zero_approx(), "sun_direction must be a finite, non-zero vector.");
	sun_direction = p_sun_direction;
}

Vector3 VolumetricCloudsEffect::get_sun_direction() const {
	return sun_direction;
}

void VolumetricCloudsEffect::set_sun_energy(float p_sun_energy) {
	ERR_FAIL_COND_MSG(!(p_sun_energy >= 0.0f), vformat("sun_energy cannot be negative, but is %f.", p_sun_energy));
	sun_energy = p_sun_energy;
}

float VolumetricCloudsEffect::get_sun_energy() const {
	return sun_energy;
}

void VolumetricCloudsEffect::set_profile_gpu(bool p_profile_gpu) {
	profile_gpu = p_profile_gpu;
}

bool VolumetricCloudsEffect::is_profiling_gpu() const {
	return profile_gpu;
}

void VolumetricCloudsEffect::reload_pipeline() {
	RenderingServer::get_singleton()->call_on_render_thread(callable_mp(this, &VolumetricCloudsEffect::_free_pipeline));
}

void VolumetricCloudsEffect::print_gpu_profile() {
	ERR_FAIL_COND_MSG(!profile_gpu, "Not currently profiling, enable profile_gpu first.");

	RenderingServer::get_singleton()->call_on_render_thread(callable_mp(this, &VolumetricCloudsEffect::_report_timestamps));
}

void VolumetricCloudsEffect::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_clouds_config", "clouds_config"), &VolumetricCloudsEffect::set_clouds_config);
	ClassDB::bind_method(D_METHOD("get_clouds_config"), &VolumetricCloudsEffect::get_clouds_config);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "clouds_config", PROPERTY_HINT_RESOURCE_TYPE, "CloudsConfig"), "set_clouds_config", "get_clouds_config");

	ClassDB::bind_method(D_METHOD("set_noise_texture", "noise_texture"), &VolumetricCloudsEffect::set_noise_texture);
	ClassDB::bind_method(D_METHOD("get_noise_texture"), &VolumetricCloudsEffect::get_noise_texture);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "noise_texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture3D"), "set_noise_texture", "get_noise_texture");

	ClassDB::bind_method(D_METHOD("set_sun_direction", "sun_direction"), &VolumetricCloudsEffect::set_sun_direction);
	ClassDB::bind_method(D_METHOD("get_sun_direction"), &VolumetricCloudsEffect::get_sun_direction);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "sun_direction"), "set_sun_direction", "get_sun_direction");

	ClassDB::bind_method(D_METHOD("set_sun_energy", "sun_energy"), &VolumetricCloudsEffect::set_sun_energy);
	ClassDB::bind_method(D_METHOD("get_sun_energy"), &VolumetricCloudsEffect::get_sun_energy);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "sun_energy", PROPERTY_HINT_RANGE, "0,100"), "set_sun_energy", "get_sun_energy");

	ClassDB::bind_method(D_METHOD("set_profile_gpu", "profile_gpu"), &VolumetricCloudsEffect::set_profile_gpu);
	ClassDB::bind_method(D_METHOD("is_profiling_gpu"), &VolumetricCloudsEffect::is_profiling_gpu);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "profile_gpu"), "set_profile_gpu", "is_profiling_gpu");

	ClassDB::bind_method(D_METHOD("reload_pipeline"), &VolumetricCloudsEffect::reload_pipeline);
	ClassDB::bind_method(D_METHOD("print_gpu_profile"), &VolumetricCloudsEffect::print_gpu_profile);
}
