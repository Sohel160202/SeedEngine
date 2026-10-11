add_executable(SeedCoreTests
    scene_tests.cpp
)
target_link_libraries(SeedCoreTests PRIVATE SeedEngine)
target_compile_features(SeedCoreTests PRIVATE cxx_std_20)
add_test(NAME SeedCoreTests COMMAND SeedCoreTests)

add_executable(SeedGameplayPersistenceTests
    gameplay_persistence_tests.cpp
)
target_link_libraries(SeedGameplayPersistenceTests PRIVATE SeedEngine)
target_compile_features(SeedGameplayPersistenceTests PRIVATE cxx_std_20)
add_test(NAME SeedGameplayPersistenceTests COMMAND SeedGameplayPersistenceTests)

add_executable(SeedGameplayRuntimeTests
    gameplay_runtime_tests.cpp
)
target_link_libraries(SeedGameplayRuntimeTests PRIVATE SeedEngine)
target_compile_features(SeedGameplayRuntimeTests PRIVATE cxx_std_20)
add_test(NAME SeedGameplayRuntimeTests COMMAND SeedGameplayRuntimeTests)

add_executable(SeedThirdPersonTests
    third_person_tests.cpp
)
target_link_libraries(SeedThirdPersonTests PRIVATE SeedEngine)
target_compile_features(SeedThirdPersonTests PRIVATE cxx_std_20)
add_test(NAME SeedThirdPersonTests COMMAND SeedThirdPersonTests)

add_executable(SeedCameraCollisionTests
    camera_collision_tests.cpp
)
target_link_libraries(SeedCameraCollisionTests PRIVATE SeedEngine)
target_compile_features(SeedCameraCollisionTests PRIVATE cxx_std_20)
add_test(NAME SeedCameraCollisionTests COMMAND SeedCameraCollisionTests)

add_executable(SeedAnimationTests
    animation_tests.cpp
)
target_link_libraries(SeedAnimationTests PRIVATE SeedEngine)
target_compile_features(SeedAnimationTests PRIVATE cxx_std_20)
add_test(NAME SeedAnimationTests COMMAND SeedAnimationTests)

add_executable(SeedAnimationScenePlaybackTests
    animation_scene_playback_tests.cpp
)
target_link_libraries(SeedAnimationScenePlaybackTests PRIVATE SeedEngine)
target_compile_features(SeedAnimationScenePlaybackTests PRIVATE cxx_std_20)
add_test(NAME SeedAnimationScenePlaybackTests COMMAND SeedAnimationScenePlaybackTests)

add_executable(SeedAssetImportTests
    asset_import_tests.cpp
)
target_link_libraries(SeedAssetImportTests PRIVATE SeedEngine)
target_compile_features(SeedAssetImportTests PRIVATE cxx_std_20)
add_test(NAME SeedAssetImportTests COMMAND SeedAssetImportTests)

add_executable(SeedAnimatedAssetImportTests
    animated_asset_import_tests.cpp
)
target_link_libraries(SeedAnimatedAssetImportTests PRIVATE SeedEngine)
target_compile_features(SeedAnimatedAssetImportTests PRIVATE cxx_std_20)
add_test(NAME SeedAnimatedAssetImportTests COMMAND SeedAnimatedAssetImportTests)

add_executable(SeedLightingPersistenceTests
    lighting_persistence_tests.cpp
)
target_link_libraries(SeedLightingPersistenceTests PRIVATE SeedEngine)
target_compile_features(SeedLightingPersistenceTests PRIVATE cxx_std_20)
add_test(NAME SeedLightingPersistenceTests COMMAND SeedLightingPersistenceTests)

add_executable(SeedRenderPersistenceTests
    render_persistence_tests.cpp
)
target_link_libraries(SeedRenderPersistenceTests PRIVATE SeedEngine)
target_compile_features(SeedRenderPersistenceTests PRIVATE cxx_std_20)
add_test(NAME SeedRenderPersistenceTests COMMAND SeedRenderPersistenceTests)

add_executable(SeedHierarchyTests
    hierarchy_tests.cpp
)
target_link_libraries(SeedHierarchyTests PRIVATE SeedEngine)
target_compile_features(SeedHierarchyTests PRIVATE cxx_std_20)
add_test(NAME SeedHierarchyTests COMMAND SeedHierarchyTests)

add_executable(SeedSceneDiffTests
    scene_diff_tests.cpp
)
target_link_libraries(SeedSceneDiffTests PRIVATE SeedEngine)
target_compile_features(SeedSceneDiffTests PRIVATE cxx_std_20)
add_test(NAME SeedSceneDiffTests COMMAND SeedSceneDiffTests)

add_executable(SeedDoctorTests
    seed_doctor_tests.cpp
)
target_link_libraries(SeedDoctorTests PRIVATE SeedEngine)
target_compile_features(SeedDoctorTests PRIVATE cxx_std_20)
add_test(NAME SeedDoctorTests COMMAND SeedDoctorTests)

# Windowed OpenGL smoke executable. It is intentionally not a normal CTest
# because headless Windows/macOS runners do not provide a display context.
add_executable(SeedSkinningRendererSmoke
    skinning_renderer_smoke.cpp
)
target_link_libraries(SeedSkinningRendererSmoke PRIVATE SeedEngine)
target_compile_features(SeedSkinningRendererSmoke PRIVATE cxx_std_20)
