#include "cloud_noise.h"

#include <cfloat>
#include <cmath>
#include <cstdint>

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/worker_thread_pool.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/templates/local_vector.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/typed_array.hpp>

namespace {

constexpr int PERLIN_OCTAVES = 4;
constexpr int PERLIN_BASE_PERIOD = 4;

constexpr int WORLEY_OCTAVES = 3;
constexpr int WORLEY_BASE_GRID = 8;

// The noise is always computed with floats, no matter the precision the engine is built with, so that a seed gives
// the same texture everywhere.
struct Float3 {
	float x;
	float y;
	float z;
};

// Wraps a cell coordinate into the period, which is what makes the noise tile.
inline uint32_t wrap_cell(int p_cell, int p_period) {
	return static_cast<uint32_t>((p_cell % p_period + p_period) % p_period);
}

inline uint32_t hash_cell(uint32_t p_x, uint32_t p_y, uint32_t p_z, uint32_t p_seed_term) {
	uint32_t hash = p_x * 73856093u ^ p_y * 19349663u ^ p_z * 83492791u ^ p_seed_term;

	hash ^= hash >> 13;
	hash *= 0x5bd1e995u;
	hash ^= hash >> 15;

	return hash;
}

inline float low_bits_to_unit(uint32_t p_hash) {
	return static_cast<float>(p_hash & 0xFFFF) / 65535.0f;
}

inline float lerp(float p_from, float p_to, float p_weight) {
	return p_from + (p_to - p_from) * p_weight;
}

inline float fade_quintic(float p_t) {
	return p_t * p_t * p_t * (p_t * (p_t * 6.0f - 15.0f) + 10.0f);
}

inline float remap(float p_value, float p_in_min, float p_in_max, float p_out_min, float p_out_max) {
	return p_out_min + (p_value - p_in_min) / (p_in_max - p_in_min) * (p_out_max - p_out_min);
}

inline uint8_t to_byte(float p_value) {
	return static_cast<uint8_t>(CLAMP(p_value, 0.0f, 1.0f) * 255.0f);
}

// The feature point of a Worley cell, as an offset inside of the cell.
Float3 cell_point(uint32_t p_x, uint32_t p_y, uint32_t p_z, uint32_t p_seed) {
	uint32_t hash = hash_cell(p_x, p_y, p_z, p_seed * 2654435761u);

	Float3 point{};
	point.x = low_bits_to_unit(hash);
	hash *= 0x27d4eb2du;
	point.y = low_bits_to_unit(hash);
	hash ^= hash >> 12;
	hash *= 0x165667b1u;
	point.z = low_bits_to_unit(hash);

	return point;
}

// A unit length gradient for a Perlin lattice point, uniformly distributed over the sphere.
Float3 cell_gradient(uint32_t p_x, uint32_t p_y, uint32_t p_z, uint32_t p_seed) {
	uint32_t hash = hash_cell(p_x, p_y, p_z, p_seed * 374761393u);

	const float u = low_bits_to_unit(hash);
	hash *= 0x27d4eb2du;
	const float v = low_bits_to_unit(hash);

	const float theta = u * 6.2831853f;
	const float phi = std::acos(2.0f * v - 1.0f);
	const float sin_phi = std::sin(phi);

	return { std::cos(theta) * sin_phi, std::sin(theta) * sin_phi, std::cos(phi) };
}

// The values of every cell of a tiling lattice. A texel only ever reads the handful of cells around it, and the cells
// are shared by a lot of texels, so computing them once up front is far cheaper than hashing them for every texel.
struct Lattice {
	LocalVector<Float3> cells;
	int period = 0;

	template <typename CellFunction>
	void fill(int p_period, uint32_t p_seed, CellFunction p_cell_function) {
		period = p_period;
		cells.resize(p_period * p_period * p_period);

		uint32_t index = 0;

		for (int z = 0; z < p_period; ++z) {
			for (int y = 0; y < p_period; ++y) {
				for (int x = 0; x < p_period; ++x) {
					cells[index++] = p_cell_function(x, y, z, p_seed);
				}
			}
		}
	}

	[[nodiscard]] inline const Float3 &get(int p_x, int p_y, int p_z) const {
		const auto size = static_cast<uint32_t>(period);

		return cells[wrap_cell(p_x, period) + size * (wrap_cell(p_y, period) + size * wrap_cell(p_z, period))];
	}
};

// Seeds are unsigned so that deriving one from another wraps around instead of overflowing.
struct NoiseLattices {
	Lattice perlin[PERLIN_OCTAVES];
	Lattice shape_worley[WORLEY_OCTAVES];
	Lattice medium_worley;
	Lattice fine_worley;

	explicit NoiseLattices(uint32_t p_seed) {
		int period = PERLIN_BASE_PERIOD;

		for (int octave = 0; octave < PERLIN_OCTAVES; ++octave) {
			perlin[octave].fill(period, p_seed + static_cast<uint32_t>(octave) * 17u, cell_gradient);
			period *= 2;
		}

		int grid = WORLEY_BASE_GRID;

		for (int octave = 0; octave < WORLEY_OCTAVES; ++octave) {
			shape_worley[octave].fill(grid, p_seed + 1000u + static_cast<uint32_t>(octave) * 17u, cell_point);
			grid *= 2;
		}

		medium_worley.fill(16, p_seed + 2000u, cell_point);
		fine_worley.fill(32, p_seed + 3000u, cell_point);
	}
};

float worley_tiling(const Float3 &p_point, const Lattice &p_points) {
	const auto grid = static_cast<float>(p_points.period);
	const Float3 scaled = { p_point.x * grid, p_point.y * grid, p_point.z * grid };

	const int base_x = static_cast<int>(std::floor(scaled.x));
	const int base_y = static_cast<int>(std::floor(scaled.y));
	const int base_z = static_cast<int>(std::floor(scaled.z));

	float min_distance = FLT_MAX;

	for (int dz = -1; dz <= 1; ++dz) {
		for (int dy = -1; dy <= 1; ++dy) {
			for (int dx = -1; dx <= 1; ++dx) {
				const int cell_x = base_x + dx;
				const int cell_y = base_y + dy;
				const int cell_z = base_z + dz;

				const Float3 &point = p_points.get(cell_x, cell_y, cell_z);

				const float offset_x = scaled.x - (static_cast<float>(cell_x) + point.x);
				const float offset_y = scaled.y - (static_cast<float>(cell_y) + point.y);
				const float offset_z = scaled.z - (static_cast<float>(cell_z) + point.z);

				min_distance = MIN(min_distance, std::sqrt(offset_x * offset_x + offset_y * offset_y + offset_z * offset_z));
			}
		}
	}

	return 1.0f - CLAMP(min_distance, 0.0f, 1.0f);
}

float perlin_tiling(const Float3 &p_point, const Lattice &p_gradients) {
	const auto period = static_cast<float>(p_gradients.period);
	const Float3 scaled = { p_point.x * period, p_point.y * period, p_point.z * period };

	const int base_x = static_cast<int>(std::floor(scaled.x));
	const int base_y = static_cast<int>(std::floor(scaled.y));
	const int base_z = static_cast<int>(std::floor(scaled.z));

	const Float3 fraction = { scaled.x - static_cast<float>(base_x), scaled.y - static_cast<float>(base_y), scaled.z - static_cast<float>(base_z) };

	const auto corner = [&](int p_dx, int p_dy, int p_dz) {
		const Float3 &gradient = p_gradients.get(base_x + p_dx, base_y + p_dy, base_z + p_dz);

		return gradient.x * (fraction.x - static_cast<float>(p_dx)) + gradient.y * (fraction.y - static_cast<float>(p_dy)) + gradient.z * (fraction.z - static_cast<float>(p_dz));
	};

	const float weight_x = fade_quintic(fraction.x);
	const float weight_y = fade_quintic(fraction.y);
	const float weight_z = fade_quintic(fraction.z);

	const float x00 = lerp(corner(0, 0, 0), corner(1, 0, 0), weight_x);
	const float x10 = lerp(corner(0, 1, 0), corner(1, 1, 0), weight_x);
	const float x01 = lerp(corner(0, 0, 1), corner(1, 0, 1), weight_x);
	const float x11 = lerp(corner(0, 1, 1), corner(1, 1, 1), weight_x);

	const float y0 = lerp(x00, x10, weight_y);
	const float y1 = lerp(x01, x11, weight_y);

	return lerp(y0, y1, weight_z) * 0.5f + 0.5f;
}

struct BakeJob {
	const NoiseLattices *lattices = nullptr;
	LocalVector<uint8_t *> slices;
	int size = 0;
};

void bake_slice(void *p_userdata, uint32_t p_z) {
	const BakeJob &job = *static_cast<const BakeJob *>(p_userdata);
	const NoiseLattices &lattices = *job.lattices;

	const auto size = static_cast<float>(job.size);
	uint8_t *texel = job.slices[p_z];

	for (int y = 0; y < job.size; ++y) {
		for (int x = 0; x < job.size; ++x) {
			const Float3 point = { static_cast<float>(x) / size, static_cast<float>(y) / size, static_cast<float>(p_z) / size };

			// The first Worley octave of the shape is also what the green channel holds.
			const float coarse_worley = worley_tiling(point, lattices.shape_worley[0]);

			float perlin = 0.0f;
			float worley = 0.0f;
			float perlin_norm = 0.0f;
			float worley_norm = 0.0f;
			float amplitude = 0.5f;

			for (const auto & octave : lattices.perlin) {
				perlin += amplitude * perlin_tiling(point, octave);
				perlin_norm += amplitude;
				amplitude *= 0.5f;
			}

			amplitude = 0.5f;

			for (int octave = 0; octave < WORLEY_OCTAVES; ++octave) {
				worley += amplitude * (octave == 0 ? coarse_worley : worley_tiling(point, lattices.shape_worley[octave]));
				worley_norm += amplitude;
				amplitude *= 0.5f;
			}

			perlin /= perlin_norm;
			worley /= worley_norm;

			*texel++ = to_byte(remap(perlin, worley - 1.0f, 1.0f, 0.0f, 1.0f));
			*texel++ = to_byte(coarse_worley);
			*texel++ = to_byte(worley_tiling(point, lattices.medium_worley));
			*texel++ = to_byte(worley_tiling(point, lattices.fine_worley));
		}
	}
}

} // namespace

Ref<ImageTexture3D> CloudNoise::bake(int p_seed, int p_size) {
	ERR_FAIL_COND_V_MSG(p_size < 1 || p_size > MAX_SIZE, Ref<ImageTexture3D>(), vformat("The size of the cloud noise must be between 1 and %d, but is %d.", MAX_SIZE, p_size));

	const NoiseLattices lattices(static_cast<uint32_t>(p_seed));

	// Every slice gets its own buffer, as each of them ends up being an image of its own anyway.
	LocalVector<PackedByteArray> buffers;
	buffers.resize(p_size);

	BakeJob job;
	job.lattices = &lattices;
	job.size = p_size;
	job.slices.resize(p_size);

	for (int z = 0; z < p_size; ++z) {
		buffers[z].resize(static_cast<int64_t>(p_size) * p_size * 4);
		job.slices[z] = buffers[z].ptrw();
	}

	WorkerThreadPool *pool = WorkerThreadPool::get_singleton();
	const int64_t group = pool->add_native_group_task(&bake_slice, &job, p_size, -1, false, "Bake cloud noise");
	pool->wait_for_group_task_completion(group);

	TypedArray<Ref<Image>> images;
	images.resize(p_size);

	for (int z = 0; z < p_size; ++z) {
		images[z] = Image::create_from_data(p_size, p_size, false, Image::FORMAT_RGBA8, buffers[z]);
	}

	Ref<ImageTexture3D> texture;
	texture.instantiate();

	const Error error = texture->create(Image::FORMAT_RGBA8, p_size, p_size, p_size, false, images);
	ERR_FAIL_COND_V_MSG(error != OK, Ref<ImageTexture3D>(), "Failed to create the cloud noise texture.");

	return texture;
}

void CloudNoise::_bind_methods() {
	ClassDB::bind_static_method("CloudNoise", D_METHOD("bake", "seed", "size"), &CloudNoise::bake, DEFVAL(DEFAULT_SIZE));

	BIND_CONSTANT(DEFAULT_SIZE);
	BIND_CONSTANT(MAX_SIZE);
}
