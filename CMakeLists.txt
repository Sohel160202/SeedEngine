cmake_minimum_required(VERSION 3.24)

project(SeedEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SEED_BUILD_TESTS "Build Seed Engine tests" ON)

include(FetchContent)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

# Keep Linux CI and the first Seed platform backend intentionally simple.
# Wayland support can be enabled later behind the same IPlatform interface.
if(UNIX AND NOT APPLE)
    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
endif()

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.5.1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glfw)

# Seed owns .seedproject/.seedscene schemas and serialization behavior.
# nlohmann/json is only the low-level JSON parser/writer behind those formats.
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# TinyGLTF is a focused import backend only. Seed owns asset IDs, imported
# resource data, the renderer resource model, project persistence and editor UX.
# Keep the stable v2 loader isolated behind Seed's AssetImporter interface.
FetchContent_Declare(
    tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG v2.9.7
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(tinygltf)
if(NOT tinygltf_POPULATED)
    FetchContent_Populate(tinygltf)
endif()

# Dear ImGui is an editor-only drawing dependency. Seed's scene, runtime,
# renderer, gameplay model and public engine API do not depend on it.
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.9b
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

add_subdirectory(engine)
add_subdirectory(runtime)
add_subdirectory(editor)

if(SEED_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
