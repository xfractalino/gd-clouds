#pragma once

#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/templates/hash_set.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>

using namespace godot;

// Assembles shader source out of reusable module files, so that code shared between shaders only exists once.
//
// Modules are plain .gdshaderinc files. A module that other modules build on has to be listed as their dependency, and
// it gets emitted before them, once.
class ShaderBuilder {
	struct Module {
		String path;
		PackedStringArray dependencies;
	};

	HashMap<String, Module> modules;
	HashSet<String> emitted;
	PackedStringArray pending;
	String result;

	bool emit(const String &p_name);

public:
	void add_module(const String &p_name, const String &p_path, const PackedStringArray &p_dependencies = PackedStringArray());

	// Builds the source for the given root modules. Returns an empty string if a module is missing, can't be loaded or
	// depends on itself.
	[[nodiscard]] String build(const PackedStringArray &p_roots);
};
