# Minimal vendored Dear ImGui; no network downloads at configure/build time.
set(IMGUI_DIR "${PROJECT_SOURCE_DIR}/third_party/imgui")
add_library(imgui STATIC
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_win32.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_dx11.cpp")
target_include_directories(imgui PUBLIC "${IMGUI_DIR}" "${IMGUI_DIR}/backends")
target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_DEMO_WINDOWS)
target_link_libraries(imgui PUBLIC d3d11 dxgi d3dcompiler dwmapi imm32)
if(MSVC)
    target_compile_options(imgui PRIVATE /utf-8 /W3 /permissive-)
endif()
