add_executable(SeedStudio
    src/main.cpp
)

target_link_libraries(SeedStudio PRIVATE SeedEngine)
target_compile_features(SeedStudio PRIVATE cxx_std_20)
