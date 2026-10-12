add_library(SeedEngine STATIC
    src/animation/AnimationSystem.cpp
    src/assets/AnimatedAssetImporter.cpp
    src/assets/AssetImporter.cpp
    src/assets/AssetRuntime.cpp
    src/core/Engine.cpp
    src/gameplay/CombatSystem.cpp
    src/gameplay/GameplayFeedbackSystem.cpp
    src/gameplay/AudioSystem.cpp
    ${miniaudio_SOURCE_DIR}/miniaudio.c
    src/diagnostics/SeedDoctor.cpp
    src/gameplay/GameplayRuntime.cpp
    src/physics/PhysicsQueries.cpp
    src/physics/PhysicsSystem.cpp
    src/platform/PlatformFactory.cpp
    src/project/ProjectSerializer.cpp
    src/project/SceneRevision.cpp
    src/project/SceneSerializer.cpp
    src/render/RendererFactory.cpp
    src/render/RenderSystem.cpp
    src/save/SaveGame.cpp
    src/save/SaveGameService.cpp
    src/scene/Hierarchy.cpp
    src/scene/Scene.cpp
    src/scene/SceneDiff.cpp
    src/scene/SceneMerge.cpp
    src/scene/SceneMergeResolution.cpp
)

target_include_directories(SeedEngine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${tinygltf_SOURCE_DIR}
        ${miniaudio_SOURCE_DIR}
)

target_compile_features(SeedEngine PUBLIC cxx_std_20)

# TinyGLTF is used as a read-only import adapter throughout Seed. Keep every
# translation unit on the same feature set as the implementation unit so inline
# TinyGLTF constructors never reference the disabled image-writer callback.
target_compile_definitions(SeedEngine PRIVATE TINYGLTF_NO_STB_IMAGE_WRITE)

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
