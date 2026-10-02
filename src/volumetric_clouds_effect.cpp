#include "volumetric_clouds_effect.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/error_macros.hpp>

VolumetricCloudsEffect::VolumetricCloudsEffect() {
	set_effect_callback_type(EFFECT_CALLBACK_TYPE_POST_TRANSPARENT);
	set_access_resolved_color(true);
	set_access_resolved_depth(true);
}

void VolumetricCloudsEffect::_render_callback(int32_t p_effect_callback_type, RenderData *p_render_data) {
	// TODO: port the ray march and upsample compute passes.
}

void VolumetricCloudsEffect::set_clouds_config(const Ref<CloudsConfig> &p_clouds_config) {
	clouds_config = p_clouds_config;
}

Ref<CloudsConfig> VolumetricCloudsEffect::get_clouds_config() const {
	return clouds_config;
}

void VolumetricCloudsEffect::set_noise_texture(const Ref<Texture3D> &p_noise_texture) {
	noise_texture = p_noise_texture;
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
}
