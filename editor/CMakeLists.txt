add_library(SeedStudioUI STATIC
    src/StudioUI.cpp
    src/StudioUIViewportBridge.cpp
    src/GameplayAuthoring.cpp
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    ${imguizmo_SOURCE_DIR}/ImGuizmo.cpp
)

target_include_directories(SeedStudioUI
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
        ${imguizmo_SOURCE_DIR}
)

target_link_libraries(SeedStudioUI
    PUBLIC
        SeedEngine
    PRIVATE
        glfw
)

target_compile_features(SeedStudioUI PRIVATE cxx_std_20)

add_executable(SeedStudio
    src/main.cpp
)

target_include_directories(SeedStudio PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(SeedStudio PRIVATE SeedEngine SeedStudioUI)
target_compile_features(SeedStudio PRIVATE cxx_std_20)
