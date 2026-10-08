add_executable(SeedRuntime
    src/main.cpp
)

target_link_libraries(SeedRuntime PRIVATE SeedEngine)
target_compile_features(SeedRuntime PRIVATE cxx_std_20)
