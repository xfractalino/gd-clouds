# Using the same minimum as the godot-cpp project
cmake_minimum_required(VERSION 3.17)

# Silence unused variable warning when specified from toolchain
if(CMAKE_C_COMPILER)
endif()

set(LIBNAME "gd_clouds") # "The name of the library"
set(GODOT_PROJECT_DIR "project" CACHE STRING "The directory of a Godot project folder")

# The addon folder inside the Godot project. It holds the .gdextension file, the binaries and the shaders.
set(ADDON_DIR "addons/${LIBNAME}")

# Make sure all the dependencies are satisfied
find_package(Python3 3.4 REQUIRED)
find_program(GIT git REQUIRED)

# Ensure godot-cpp submodule has been updated
if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/godot-cpp/src")
    message(NOTICE "godot-cpp bindings source not found")
    message(NOTICE "initializing/updating the godot-cpp submodule...")

    # update the c++ bindings submodule to populate it with the necessary source for the library
    execute_process(
        COMMAND git submodule update --init godot-cpp
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        COMMAND_ERROR_IS_FATAL ANY
    )
endif()

# Godot API version to target, unless overridden with -DGODOTCPP_API_VERSION=<version>
set(GODOTCPP_DEFAULT_API_VERSION "4.5")
add_subdirectory(godot-cpp SYSTEM)

# When compiling for emscripten, this toolchainhack needs to be included.
# set(CMAKE_PROJECT_<project-name>_INCLUDE ${godot-cpp_SOURCE_DIR}/cmake/emsdkHack.cmake)

# Create the gen directory at configure time so IDEs can see generated headers immediately after generate_bindings
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/godot-cpp/gen/include")

# Add godot-cpp's module path and include the exported functions.
# This is made available for documentation generation
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${godot-cpp_SOURCE_DIR}/cmake")
include(GodotCPPModule)

# The godot-cpp target has some of useful properties attached that can be retrieved like so.
get_target_property(GODOTCPP_SUFFIX godot::cpp GODOTCPP_SUFFIX)
get_target_property(GODOTCPP_PLATFORM godot::cpp GODOTCPP_PLATFORM)

# Now we can specify our own project which will inherit any global cmake properties or variables that have been defined.
project(gd-clouds
    VERSION 0.1
    DESCRIPTION "Volumetric clouds compositor effect for Godot's RenderingDevice renderers."
    HOMEPAGE_URL "https://github.com/xfractalino/gd-clouds"
    LANGUAGES CXX
)

# godot-cpp only generates its bindings when it gets built, but IDEs index the include directories as soon as the
# project is loaded. CLion doesn't pick up the headers that show up afterwards until its caches are invalidated, so
# generate them here already when they are missing. The build still generates them itself as usual.
function(generate_bindings_for_ide)
    set(OUTPUT_DIR "${godot-cpp_BINARY_DIR}")

    if(EXISTS "${OUTPUT_DIR}/gen/include/godot_cpp/classes/object.hpp")
        return()
    endif()

    # This mirrors how godot-cpp/cmake/godotcpp.cmake picks the inputs of its binding generator.
    if(GODOTCPP_BUILD_PROFILE)
        set(API_FILE "${OUTPUT_DIR}/extension_api.json")
    elseif(GODOTCPP_CUSTOM_API_FILE)
        set(API_FILE "${GODOTCPP_CUSTOM_API_FILE}")
    else()
        set(API_VERSION "${GODOTCPP_API_VERSION}")
        if(API_VERSION STREQUAL "")
            set(API_VERSION "${GODOTCPP_DEFAULT_API_VERSION}")
        endif()
        string(REPLACE "." "-" API_VERSION "${API_VERSION}")
        set(API_FILE "${GODOTCPP_GDEXTENSION_DIR}/extension_api-${API_VERSION}.json")
    endif()

    set(USE_TEMPLATE_GET_NODE "False")
    if(GODOTCPP_GENERATE_TEMPLATE_GET_NODE)
        set(USE_TEMPLATE_GET_NODE "True")
    endif()

    math(EXPR BITS "${CMAKE_SIZEOF_VOID_P} * 8")
    get_target_property(PRECISION godot::cpp GODOTCPP_PRECISION)

    string(CONCAT PYTHON_SCRIPT
        "from binding_generator import generate_bindings; "
        "generate_bindings("
        "api_filepath='${API_FILE}', "
        "interface_filepath='${GODOTCPP_GDEXTENSION_DIR}/gdextension_interface.json', "
        "use_template_get_node='${USE_TEMPLATE_GET_NODE}', "
        "bits='${BITS}', "
        "precision='${PRECISION}', "
        "output_dir='${OUTPUT_DIR}', "
        "hooks_path='${GODOTCPP_BINDING_HOOK_FILE}')"
    )

    message(STATUS "Generating the godot-cpp bindings ahead of the build, for IDEs")

    execute_process(
        COMMAND "${Python3_EXECUTABLE}" -c "${PYTHON_SCRIPT}"
        WORKING_DIRECTORY "${godot-cpp_SOURCE_DIR}"
        RESULT_VARIABLE RESULT
        OUTPUT_QUIET
        ERROR_VARIABLE ERRORS
    )

    # This is only a convenience, the build doesn't depend on it.
    if(NOT RESULT EQUAL 0)
        message(WARNING "Could not generate the godot-cpp bindings ahead of the build:\n${ERRORS}")
    endif()
endfunction()

generate_bindings_for_ide()

add_library(${LIBNAME} SHARED)

target_sources(${LIBNAME}
    PRIVATE
    src/register_types.cpp
    src/register_types.h
    src/cloud_noise.cpp
    src/cloud_noise.h
    src/clouds_config.cpp
    src/clouds_config.h
    src/rd_utils.h
    src/shader_builder.cpp
    src/shader_builder.h
    src/volumetric_clouds_effect.cpp
    src/volumetric_clouds_effect.h
)

# Fetch a list of the xml files to use for documentation and add to our target
file(GLOB_RECURSE DOC_XML LIST_DIRECTORIES NO CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/doc_classes/*.xml")

# conditionally add doc data to compile output
if(DOC_XML)
    if(GODOTCPP_TARGET MATCHES "editor|template_debug")
        target_doc_sources(${LIBNAME} "${DOC_XML}")
    endif()
endif()

target_link_libraries(${LIBNAME} PRIVATE godot-cpp)

# Require at least C++17 for this target
set_property(TARGET ${LIBNAME} PROPERTY CXX_STANDARD 17)

set_target_properties(${LIBNAME}
    PROPERTIES
    # The generator expression here prevents msvc from adding a Debug or Release subdir.
    RUNTIME_OUTPUT_DIRECTORY "$<1:${PROJECT_SOURCE_DIR}/bin/${GODOTCPP_PLATFORM}>"
    LIBRARY_OUTPUT_DIRECTORY "$<1:${PROJECT_SOURCE_DIR}/bin/${GODOTCPP_PLATFORM}>"

    OUTPUT_NAME "${LIBNAME}${GODOTCPP_SUFFIX}"
)

# Match the file names the SCons build produces, which are the ones listed in the .gdextension file: no prefix on
# Windows, "lib" everywhere else.
if(WIN32)
    set_target_properties(${LIBNAME} PROPERTIES PREFIX "")
else()
    set_target_properties(${LIBNAME} PROPERTIES PREFIX "lib")
endif()

set(GODOT_PROJECT_BINARY_DIR "${PROJECT_SOURCE_DIR}/${GODOT_PROJECT_DIR}/${ADDON_DIR}/bin/${GODOTCPP_PLATFORM}")

add_custom_command(TARGET ${LIBNAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${LIBNAME}>" "${GODOT_PROJECT_BINARY_DIR}/$<TARGET_FILE_NAME:${LIBNAME}>"
)
