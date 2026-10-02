#include "shader_builder.h"

#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/shader_include.hpp>
#include <godot_cpp/core/error_macros.hpp>

namespace {

bool load_module_code(const String &p_path, String &r_code) {
	// Outside of exported projects the modules are read from disk again every time, so that edits to them are picked
	// up when the shader gets rebuilt.
	const ResourceLoader::CacheMode cache_mode = OS::get_singleton()->has_feature("editor") ? ResourceLoader::CACHE_MODE_IGNORE : ResourceLoader::CACHE_MODE_REUSE;

	const Ref<ShaderInclude> include = ResourceLoader::get_singleton()->load(p_path, "ShaderInclude", cache_mode);
	ERR_FAIL_COND_V_MSG(include.is_null(), false, vformat("Failed to load shader module file: %s", p_path));

	r_code = include->get_code();

	return true;
}

} // namespace

void ShaderBuilder::add_module(const String &p_name, const String &p_path, const PackedStringArray &p_dependencies) {
	modules[p_name] = Module{ p_path, p_dependencies };
}

String ShaderBuilder::build(const PackedStringArray &p_roots) {
	result = String();
	emitted.clear();
	pending.clear();

	for (const String &root : p_roots) {
		if (!emit(root)) {
			return String();
		}
	}

	return result;
}

bool ShaderBuilder::emit(const String &p_name) {
	if (emitted.has(p_name)) {
		return true;
	}

	const Module *module = modules.getptr(p_name);
	ERR_FAIL_NULL_V_MSG(module, false, vformat("Shader module '%s' has not been registered.", p_name));

	ERR_FAIL_COND_V_MSG(pending.has(p_name), false, vformat("Cyclic shader module dependency: %s -> %s", String(" -> ").join(pending), p_name));

	pending.push_back(p_name);

	for (const String &dependency : module->dependencies) {
		if (!emit(dependency)) {
			return false;
		}
	}

	pending.remove_at(pending.size() - 1);
	emitted.insert(p_name);

	String code;

	if (!load_module_code(module->path, code)) {
		return false;
	}

	result += "// module: " + p_name + "\n" + code + "\n";

	return true;
}
