add_library(SeedEngine STATIC
    src/core/Engine.cpp
    src/platform/PlatformFactory.cpp
    src/project/ProjectSerializer.cpp
    src/project/SceneSerializer.cpp
    src/render/RendererFactory.cpp
    src/render/RenderSystem.cpp
    src/scene/Scene.cpp
)

target_include_directories(SeedEngine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(SeedEngine PUBLIC cxx_std_20)

target_link_libraries(SeedEngine
    PRIVATE
        glfw
        nlohmann_json::nlohmann_json
)

if(MSVC)
    target_compile_options(SeedEngine PRIVATE /W4 /permissive-)
else()
    target_compile_options(SeedEngine PRIVATE -Wall -Wextra -Wpedantic)
endif()
