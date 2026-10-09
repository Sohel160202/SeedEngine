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
