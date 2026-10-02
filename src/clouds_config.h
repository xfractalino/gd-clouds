#pragma once

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/vector3.hpp>

using namespace godot;

// Configuration container for a cloud layer, which is a spherical shell around a planet.
//
// The setters reject values the ray marcher can't work with and leave the previous value in place. The one thing they
// can't check is that the outer height is above the inner one, as properties are assigned one at a time and a scene
// file may set them in either order. The effect handles that case when it reads the heights.
class CloudsConfig : public Resource {
	GDCLASS(CloudsConfig, Resource)

	Vector3 planet_center;
	float planet_radius = 1000.0f;
	float cloud_inner_height = 10.0f;
	float cloud_outer_height = 50.0f;
	int seed = 1234;
	float cloud_tile_size = 200.0f;
	float density_multiplier = 1.0f;
	float coverage = 0.3f;
	int march_steps = 64;
	int light_steps = 6;
	float max_march_distance = 8000.0f;
	int resolution_divisor = 2;

protected:
	static void _bind_methods();

public:
	void set_planet_center(const Vector3 &p_planet_center);
	Vector3 get_planet_center() const;

	void set_planet_radius(float p_planet_radius);
	[[nodiscard]] float get_planet_radius() const;

	void set_cloud_inner_height(float p_cloud_inner_height);
	[[nodiscard]] float get_cloud_inner_height() const;

	void set_cloud_outer_height(float p_cloud_outer_height);
	[[nodiscard]] float get_cloud_outer_height() const;

	void set_seed(int p_seed);
	[[nodiscard]] int get_seed() const;

	void set_cloud_tile_size(float p_cloud_tile_size);
	[[nodiscard]] float get_cloud_tile_size() const;

	void set_density_multiplier(float p_density_multiplier);
	[[nodiscard]] float get_density_multiplier() const;

	void set_coverage(float p_coverage);
	[[nodiscard]] float get_coverage() const;

	void set_march_steps(int p_march_steps);
	[[nodiscard]] int get_march_steps() const;

	void set_light_steps(int p_light_steps);
	[[nodiscard]] int get_light_steps() const;

	void set_max_march_distance(float p_max_march_distance);
	[[nodiscard]] float get_max_march_distance() const;

	void set_resolution_divisor(int p_resolution_divisor);
	[[nodiscard]] int get_resolution_divisor() const;
};
