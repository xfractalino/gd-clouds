#pragma once

#include <godot_cpp/classes/image_texture3d.hpp>
#include <godot_cpp/classes/object.hpp>

using namespace godot;

// Generator of the tiling 3D noise the clouds get their density from.
class CloudNoise : public Object {
	GDCLASS(CloudNoise, Object)

protected:
	static void _bind_methods();

public:
	static constexpr int DEFAULT_SIZE = 128;

	// The texture takes size^3 * 4 bytes, which is 512 MiB at this size.
	static constexpr int MAX_SIZE = 512;

	// Bakes a cube of noise that tiles along every axis. The red channel holds the Perlin-Worley noise that gives the
	// clouds their shape. The green, blue and alpha channels hold Worley noise of 8, 16 and 32 cells per tile, which
	// erodes their edges.
	//
	// This is computed on the CPU, spread over the worker threads, and blocks until it is done.
	[[nodiscard]] static Ref<ImageTexture3D> bake(int p_seed, int p_size = DEFAULT_SIZE);
};
