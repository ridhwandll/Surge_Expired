include(${CMAKE_SOURCE_DIR}/scripts/CMakeUtils.cmake)

if(WIN32)
    set(BACKENDS
        "Backends/imgui_impl_vulkan.cpp"
        "Backends/imgui_impl_vulkan.h"
        "Backends/imgui_impl_win32.cpp"
        "Backends/imgui_impl_win32.h")
endif(WIN32)

if(ANDROID)
    set(BACKENDS
        "Backends/imgui_impl_vulkan.cpp"
        "Backends/imgui_impl_vulkan.h"
        "Backends/imgui_impl_android.cpp"
        "Backends/imgui_impl_android.h")
endif()



add_library(ImGui
    "imconfig.h"
    "imgui.h"
    "imgui.cpp"
    "imgui_draw.cpp"
    "imgui_internal.h"
    "imgui_widgets.cpp"
    "imstb_rectpack.h"
    "imstb_textedit.h"
    "imstb_truetype.h"
    "imgui_demo.cpp"
    "imgui_tables.cpp"
    "imgui_stdlib.h"
    "imgui_stdlib.cpp"
    "ImGuizmo.cpp"
    "ImGuizmo.h"
    ${BACKENDS}
)

target_include_directories(ImGui PRIVATE
    .
    ${CMAKE_SOURCE_DIR}/Engine/Vendor/volk
    ${CMAKE_SOURCE_DIR}/Engine/Vendor/glm
    ${CMAKE_SOURCE_DIR}/Engine/Vendor/Vulkan-Headers/Include
)

target_compile_definitions(ImGui
    PRIVATE
    IMGUI_IMPL_VULKAN_USE_VOLK
)

set_target_properties(ImGui PROPERTIES FOLDER Dependencies)
