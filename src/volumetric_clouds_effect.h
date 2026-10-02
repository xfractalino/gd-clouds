#pragma once

#include <godot_cpp/classes/compositor_effect.hpp>
#include <godot_cpp/classes/rd_uniform.hpp>
#include <godot_cpp/classes/render_data.hpp>
#include <godot_cpp/classes/render_scene_buffers_rd.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/texture3d.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "clouds_config.h"

using namespace godot;

// Volumetric clouds rendered with compute shaders, as a compositor effect. This needs a RenderingDevice, so it doesn't
// work on the Compatibility renderer.
//
// The clouds are ray marched into a texture of reduced resolution, which a second pass then upsamples and composites
// over the scene.
class VolumetricCloudsEffect : public CompositorEffect {
	GDCLASS(VolumetricCloudsEffect, CompositorEffect)

	enum PipelineState {
		PIPELINE_UNINITIALIZED,
		PIPELINE_READY,
		// Building the pipeline went wrong. It isn't attempted again until a reload is requested, as it would just
		// fail in the same way every frame.
		PIPELINE_FAILED,
	};

	static constexpr uint32_t PUSH_CONSTANTS_SIZE = 128;
	static constexpr uint32_t PARAMS_BUFFER_SIZE = 128;

	Ref<CloudsConfig> clouds_config;
	Ref<Texture3D> noise_texture;

	Vector3 sun_direction = Vector3(0.4f, 0.8f, 0.3f).normalized();
	float sun_energy = 25.0f;

	bool profile_gpu = false;

	const StringName cloud_context = "volumetric_clouds";
	const StringName cloud_texture_name = "cloud_half";
	const StringName render_buffers_context = "render_buffers";
	const StringName color_texture_name = "color";
	const StringName depth_texture_name = "depth";

	// What gets rendered when no config is set.
	Ref<CloudsConfig> default_config;

	PipelineState pipeline_state = PIPELINE_UNINITIALIZED;

	RenderingDevice *rendering_device = nullptr;

	RID ray_marcher_shader;
	RID ray_marcher_pipeline;
	RID upsampler_shader;
	RID upsampler_pipeline;
	RID depth_sampler;
	RID noise_sampler;
	RID params_buffer;

	// The uniforms are kept around and only pointed at the textures of the current frame, instead of being created
	// again for every view.
	Ref<RDUniform> march_target_uniform;
	Ref<RDUniform> march_depth_uniform;
	Ref<RDUniform> march_noise_uniform;
	Ref<RDUniform> march_params_uniform;
	Ref<RDUniform> upsample_color_uniform;
	Ref<RDUniform> upsample_depth_uniform;
	Ref<RDUniform> upsample_cloud_uniform;
	Ref<RDUniform> upsample_params_uniform;

	TypedArray<Ref<RDUniform>> march_uniforms;
	TypedArray<Ref<RDUniform>> upsample_uniforms;

	PackedByteArray push_constants;
	PackedByteArray params;

	Vector2i cloud_texture_size;
	uint32_t cloud_texture_views = 0;

	float last_checked_inner_height = -1.0f;
	float last_checked_outer_height = -1.0f;

	bool missing_noise_reported = false;

	bool _initialize_pipeline();
	void _free_pipeline();
	static void _free_rids(const Array &p_rids);
	[[nodiscard]] Array _get_owned_rids() const;

	void _ensure_cloud_texture(const Ref<RenderSceneBuffersRD> &p_scene_buffers, const Vector2i &p_march_size, uint32_t p_view_count);
	void _update_params(const CloudsConfig &p_config, const Vector2i &p_full_size, const Vector2i &p_march_size, const Vector3 &p_camera_position);
	void _report_timestamps();

protected:
	static void _bind_methods();

public:
	VolumetricCloudsEffect();
	~VolumetricCloudsEffect() override;

	void _render_callback(int32_t p_effect_callback_type, RenderData *p_render_data) override;

	void set_clouds_config(const Ref<CloudsConfig> &p_clouds_config);
	[[nodiscard]] Ref<CloudsConfig> get_clouds_config() const;

	void set_noise_texture(const Ref<Texture3D> &p_noise_texture);
	[[nodiscard]] Ref<Texture3D> get_noise_texture() const;

	void set_sun_direction(const Vector3 &p_sun_direction);
	Vector3 get_sun_direction() const;

	void set_sun_energy(float p_sun_energy);
	[[nodiscard]] float get_sun_energy() const;

	void set_profile_gpu(bool p_profile_gpu);
	[[nodiscard]] bool is_profiling_gpu() const;

	// Throws the pipeline away, so that it gets built again from the shader files the next time the clouds are rendered.
	void reload_pipeline();

	// Prints how long the passes took on the GPU during the last frame. Needs GPU profiling to be enabled.
	void print_gpu_profile();
};
