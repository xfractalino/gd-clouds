#include "clouds_config.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/error_macros.hpp>

// The range checks are written as a negated "is valid" condition so that NaN is rejected as well, as every comparison
// with NaN is false.

void CloudsConfig::set_planet_center(const Vector3 &p_planet_center) {
	ERR_FAIL_COND_MSG(!p_planet_center.is_finite(), "planet_center must be finite.");
	planet_center = p_planet_center;
	emit_changed();
}

Vector3 CloudsConfig::get_planet_center() const {
	return planet_center;
}

void CloudsConfig::set_planet_radius(float p_planet_radius) {
	ERR_FAIL_COND_MSG(!(p_planet_radius > 0.0f), vformat("planet_radius must be positive, but is %f.", p_planet_radius));
	planet_radius = p_planet_radius;
	emit_changed();
}

float CloudsConfig::get_planet_radius() const {
	return planet_radius;
}

void CloudsConfig::set_cloud_inner_height(float p_cloud_inner_height) {
	ERR_FAIL_COND_MSG(!(p_cloud_inner_height >= 0.0f), vformat("cloud_inner_height is an altitude above the ground so it cannot be negative, but is %f.", p_cloud_inner_height));
	cloud_inner_height = p_cloud_inner_height;
	emit_changed();
}

float CloudsConfig::get_cloud_inner_height() const {
	return cloud_inner_height;
}

void CloudsConfig::set_cloud_outer_height(float p_cloud_outer_height) {
	ERR_FAIL_COND_MSG(!(p_cloud_outer_height >= 0.0f), vformat("cloud_outer_height is an altitude above the ground so it cannot be negative, but is %f.", p_cloud_outer_height));
	cloud_outer_height = p_cloud_outer_height;
	emit_changed();
}

float CloudsConfig::get_cloud_outer_height() const {
	return cloud_outer_height;
}

void CloudsConfig::set_seed(int p_seed) {
	seed = p_seed;
	emit_changed();
}

int CloudsConfig::get_seed() const {
	return seed;
}

void CloudsConfig::set_cloud_tile_size(float p_cloud_tile_size) {
	ERR_FAIL_COND_MSG(!(p_cloud_tile_size > 0.0f), vformat("cloud_tile_size must be positive, but is %f.", p_cloud_tile_size));
	cloud_tile_size = p_cloud_tile_size;
	emit_changed();
}

float CloudsConfig::get_cloud_tile_size() const {
	return cloud_tile_size;
}

void CloudsConfig::set_density_multiplier(float p_density_multiplier) {
	ERR_FAIL_COND_MSG(!(p_density_multiplier >= 0.0f && p_density_multiplier <= 1.0f), vformat("density_multiplier must be between 0 and 1, but is %f.", p_density_multiplier));
	density_multiplier = p_density_multiplier;
	emit_changed();
}

float CloudsConfig::get_density_multiplier() const {
	return density_multiplier;
}

void CloudsConfig::set_coverage(float p_coverage) {
	ERR_FAIL_COND_MSG(!(p_coverage >= 0.1f && p_coverage <= 1.0f), vformat("coverage must be between 0.1 and 1, but is %f.", p_coverage));
	coverage = p_coverage;
	emit_changed();
}

float CloudsConfig::get_coverage() const {
	return coverage;
}

void CloudsConfig::set_march_steps(int p_march_steps) {
	ERR_FAIL_COND_MSG(p_march_steps < 1 || p_march_steps > 256, vformat("march_steps must be between 1 and 256, but is %d.", p_march_steps));
	march_steps = p_march_steps;
	emit_changed();
}

int CloudsConfig::get_march_steps() const {
	return march_steps;
}

void CloudsConfig::set_light_steps(int p_light_steps) {
	ERR_FAIL_COND_MSG(p_light_steps < 1 || p_light_steps > 10, vformat("light_steps must be between 1 and 10, but is %d.", p_light_steps));
	light_steps = p_light_steps;
	emit_changed();
}

int CloudsConfig::get_light_steps() const {
	return light_steps;
}

void CloudsConfig::set_max_march_distance(float p_max_march_distance) {
	ERR_FAIL_COND_MSG(!(p_max_march_distance > 0.0f), vformat("max_march_distance must be positive, but is %f.", p_max_march_distance));
	max_march_distance = p_max_march_distance;
	emit_changed();
}

float CloudsConfig::get_max_march_distance() const {
	return max_march_distance;
}

void CloudsConfig::set_resolution_divisor(int p_resolution_divisor) {
	ERR_FAIL_COND_MSG(p_resolution_divisor < 1 || p_resolution_divisor > 4, vformat("resolution_divisor must be between 1 and 4, but is %d.", p_resolution_divisor));
	resolution_divisor = p_resolution_divisor;
	emit_changed();
}

int CloudsConfig::get_resolution_divisor() const {
	return resolution_divisor;
}

void CloudsConfig::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_planet_center", "planet_center"), &CloudsConfig::set_planet_center);
	ClassDB::bind_method(D_METHOD("get_planet_center"), &CloudsConfig::get_planet_center);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "planet_center"), "set_planet_center", "get_planet_center");

	ClassDB::bind_method(D_METHOD("set_planet_radius", "planet_radius"), &CloudsConfig::set_planet_radius);
	ClassDB::bind_method(D_METHOD("get_planet_radius"), &CloudsConfig::get_planet_radius);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "planet_radius"), "set_planet_radius", "get_planet_radius");

	ClassDB::bind_method(D_METHOD("set_cloud_inner_height", "cloud_inner_height"), &CloudsConfig::set_cloud_inner_height);
	ClassDB::bind_method(D_METHOD("get_cloud_inner_height"), &CloudsConfig::get_cloud_inner_height);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "cloud_inner_height"), "set_cloud_inner_height", "get_cloud_inner_height");

	ClassDB::bind_method(D_METHOD("set_cloud_outer_height", "cloud_outer_height"), &CloudsConfig::set_cloud_outer_height);
	ClassDB::bind_method(D_METHOD("get_cloud_outer_height"), &CloudsConfig::get_cloud_outer_height);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "cloud_outer_height"), "set_cloud_outer_height", "get_cloud_outer_height");

	ClassDB::bind_method(D_METHOD("set_seed", "seed"), &CloudsConfig::set_seed);
	ClassDB::bind_method(D_METHOD("get_seed"), &CloudsConfig::get_seed);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "seed"), "set_seed", "get_seed");

	ClassDB::bind_method(D_METHOD("set_cloud_tile_size", "cloud_tile_size"), &CloudsConfig::set_cloud_tile_size);
	ClassDB::bind_method(D_METHOD("get_cloud_tile_size"), &CloudsConfig::get_cloud_tile_size);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "cloud_tile_size", PROPERTY_HINT_RANGE, "1,1000,1,or_greater"), "set_cloud_tile_size", "get_cloud_tile_size");

	ClassDB::bind_method(D_METHOD("set_density_multiplier", "density_multiplier"), &CloudsConfig::set_density_multiplier);
	ClassDB::bind_method(D_METHOD("get_density_multiplier"), &CloudsConfig::get_density_multiplier);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "density_multiplier", PROPERTY_HINT_RANGE, "0.0,1.0"), "set_density_multiplier", "get_density_multiplier");

	ClassDB::bind_method(D_METHOD("set_coverage", "coverage"), &CloudsConfig::set_coverage);
	ClassDB::bind_method(D_METHOD("get_coverage"), &CloudsConfig::get_coverage);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "coverage", PROPERTY_HINT_RANGE, "0.1,1.0"), "set_coverage", "get_coverage");

	ClassDB::bind_method(D_METHOD("set_march_steps", "march_steps"), &CloudsConfig::set_march_steps);
	ClassDB::bind_method(D_METHOD("get_march_steps"), &CloudsConfig::get_march_steps);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "march_steps", PROPERTY_HINT_RANGE, "1,256"), "set_march_steps", "get_march_steps");

	ClassDB::bind_method(D_METHOD("set_light_steps", "light_steps"), &CloudsConfig::set_light_steps);
	ClassDB::bind_method(D_METHOD("get_light_steps"), &CloudsConfig::get_light_steps);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "light_steps", PROPERTY_HINT_RANGE, "1,10"), "set_light_steps", "get_light_steps");

	ClassDB::bind_method(D_METHOD("set_max_march_distance", "max_march_distance"), &CloudsConfig::set_max_march_distance);
	ClassDB::bind_method(D_METHOD("get_max_march_distance"), &CloudsConfig::get_max_march_distance);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_march_distance", PROPERTY_HINT_RANGE, "0,10000,1,or_greater"), "set_max_march_distance", "get_max_march_distance");

	ClassDB::bind_method(D_METHOD("set_resolution_divisor", "resolution_divisor"), &CloudsConfig::set_resolution_divisor);
	ClassDB::bind_method(D_METHOD("get_resolution_divisor"), &CloudsConfig::get_resolution_divisor);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "resolution_divisor", PROPERTY_HINT_RANGE, "1,4,1"), "set_resolution_divisor", "get_resolution_divisor");
}
