#pragma once

#include <godot_cpp/classes/rd_uniform.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/variant/projection.hpp>
#include <godot_cpp/variant/rid.hpp>

using namespace godot;

// Boilerplate for talking to a RenderingDevice.
namespace rd_utils {

// Updates an image uniform given its shader binding and a texture RID.
inline void update_image(const Ref<RDUniform> &p_uniform, int p_binding, const RID &p_texture) {
	p_uniform->clear_ids();
	p_uniform->set_uniform_type(RenderingDevice::UNIFORM_TYPE_IMAGE);
	p_uniform->set_binding(p_binding);
	p_uniform->add_id(p_texture);
}

// Updates a sampler uniform given its shader binding, a sampler and a texture RID.
inline void update_sampled(const Ref<RDUniform> &p_uniform, int p_binding, const RID &p_sampler, const RID &p_texture) {
	p_uniform->clear_ids();
	p_uniform->set_uniform_type(RenderingDevice::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE);
	p_uniform->set_binding(p_binding);
	p_uniform->add_id(p_sampler);
	p_uniform->add_id(p_texture);
}

// Updates a uniform buffer uniform given its shader binding and its buffer RID.
inline void update_uniform_buffer(const Ref<RDUniform> &p_uniform, int p_binding, const RID &p_buffer) {
	p_uniform->clear_ids();
	p_uniform->set_uniform_type(RenderingDevice::UNIFORM_TYPE_UNIFORM_BUFFER);
	p_uniform->set_binding(p_binding);
	p_uniform->add_id(p_buffer);
}

// Writes a vec4 and returns where the next value goes. Shaders take floats no matter the precision the engine is built
// with, which is why this doesn't take a Vector4.
inline float *write_vec4(float *p_destination, float p_x, float p_y, float p_z, float p_w) {
	p_destination[0] = p_x;
	p_destination[1] = p_y;
	p_destination[2] = p_z;
	p_destination[3] = p_w;

	return p_destination + 4;
}

// Writes a mat4, column by column, and returns where the next value goes.
inline float *write_mat4(float *p_destination, const Projection &p_matrix) {
	for (const Vector4 &column : p_matrix.columns) {
		p_destination = write_vec4(p_destination, static_cast<float>(column.x), static_cast<float>(column.y), static_cast<float>(column.z), static_cast<float>(column.w));
	}

	return p_destination;
}

} // namespace rd_utils
