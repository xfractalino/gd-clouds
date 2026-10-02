#pragma once

#include <godot_cpp/classes/compositor_effect.hpp>
#include <godot_cpp/classes/render_data.hpp>
#include <godot_cpp/classes/texture3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "clouds_config.h"

using namespace godot;

// Volumetric clouds rendered with compute shaders, as a compositor effect. This needs a RenderingDevice, so it doesn't
// work on the Compatibility renderer.
class VolumetricCloudsEffect : public CompositorEffect {
	GDCLASS(VolumetricCloudsEffect, CompositorEffect)

	Ref<CloudsConfig> clouds_config;
	Ref<Texture3D> noise_texture;

	Vector3 sun_direction = Vector3(0.4f, 0.8f, 0.3f).normalized();
	float sun_energy = 25.0f;

protected:
	static void _bind_methods();

public:
	VolumetricCloudsEffect();

	void _render_callback(int32_t p_effect_callback_type, RenderData *p_render_data) override;

	void set_clouds_config(const Ref<CloudsConfig> &p_clouds_config);
	[[nodiscard]] Ref<CloudsConfig> get_clouds_config() const;

	void set_noise_texture(const Ref<Texture3D> &p_noise_texture);
	[[nodiscard]] Ref<Texture3D> get_noise_texture() const;

	void set_sun_direction(const Vector3 &p_sun_direction);
	Vector3 get_sun_direction() const;

	void set_sun_energy(float p_sun_energy);
	[[nodiscard]] float get_sun_energy() const;
};
