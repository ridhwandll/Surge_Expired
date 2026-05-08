cmake_minimum_required(VERSION 3.21)
project(SurgePlayer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)

# -----------------------------------------------------------------------
# Paths
# -----------------------------------------------------------------------
set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../..")
get_filename_component(REPO_ROOT "${REPO_ROOT}" ABSOLUTE)

set(ENGINE_ROOT "${REPO_ROOT}/Engine")
set(ENGINE_SRC_ROOT "${REPO_ROOT}/Engine/Source")
set(VENDOR_ROOT "${REPO_ROOT}/Engine/Vendor")
set(PLAYER_SRC_ROOT "${REPO_ROOT}/Player/Source")

# -----------------------------------------------------------------------
# android_native_app_glue (shipped with the NDK)
# -----------------------------------------------------------------------
set(APP_GLUE_DIR "${ANDROID_NDK}/sources/android/native_app_glue")

# -----------------------------------------------------------------------
# Collect engine sources – exclude all Windows-specific files
# -----------------------------------------------------------------------
file(GLOB_RECURSE ENGINE_SOURCES
    "${ENGINE_SRC_ROOT}/*.cpp" "${ENGINE_SRC_ROOT}/*.hpp"
)
list(FILTER ENGINE_SOURCES EXCLUDE REGEX ".*/Platform/Windows/.*")

# Vendor helper source (header-only library with a single .cpp)
list(APPEND ENGINE_SOURCES "${VENDOR_ROOT}/stb/stb_image.cpp" "${VENDOR_ROOT}/cgltf/cgltf.cpp")

# -----------------------------------------------------------------------
# Player sources
# -----------------------------------------------------------------------
file(GLOB PLAYER_SOURCES
    "${PLAYER_SRC_ROOT}/*.cpp"
)

# -----------------------------------------------------------------------
# Shared library target (APK packages .so files, not executables)
# -----------------------------------------------------------------------
add_library(SurgePlayer SHARED
    ${ENGINE_SOURCES}
    ${PLAYER_SOURCES}
    "${APP_GLUE_DIR}/android_native_app_glue.c"
)

target_link_options(SurgePlayer PRIVATE
        "-Wl,-z,max-page-size=16384"
)

# -----------------------------------------------------------------------
# Include directories
# -----------------------------------------------------------------------
target_include_directories(SurgePlayer PUBLIC
    "${ENGINE_ROOT}"
    "${ENGINE_SRC_ROOT}"
    "${ENGINE_SRC_ROOT}/SurgeReflect/Include"
    "${VENDOR_ROOT}"
    "${VENDOR_ROOT}/fmt/Include"
    "${VENDOR_ROOT}/glm"
    "${VENDOR_ROOT}/Vulkan-Headers/Include"
    "${VENDOR_ROOT}/VulkanMemoryAllocator/Include"
    "${VENDOR_ROOT}/volk"
    "${VENDOR_ROOT}/SPIRV-Cross/Include"
    "${VENDOR_ROOT}/stb"
    "${VENDOR_ROOT}/cgltf"
    "${VENDOR_ROOT}/ImGui"
    "${VENDOR_ROOT}/Optick/Include"
    "${VENDOR_ROOT}/entt/Include"
    "${VENDOR_ROOT}/json/Include"
    "${VENDOR_ROOT}/FontAwesome"
    "${APP_GLUE_DIR}"
    "${PLAYER_SRC_ROOT}"
)

# -----------------------------------------------------------------------
# Platform defines
# -----------------------------------------------------------------------
target_compile_definitions(SurgePlayer PRIVATE
    SURGE_PLATFORM_ANDROID
    VK_USE_PLATFORM_ANDROID_KHR
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_FORCE_RADIANS
    _CRT_SECURE_NO_WARNINGS
    $<$<CONFIG:Debug>:SURGE_DEBUG>
    $<$<CONFIG:Release>:SURGE_RELEASE>
    $<$<CONFIG:RelWithDebInfo>:SURGE_RELEASE>
    SURGE_EXPORT
)

# ---------------------------------------------------------------------
# NDK system libraries
# ----------------------------------------------------------------------
find_library(log-lib log)
find_library(android-lib android)
find_library(vulkan-lib vulkan)

# Build vendored static libraries from source

# volk (single-header loader; compile it as a static lib)
add_library(volk_android STATIC "${VENDOR_ROOT}/volk/volk.c")
target_include_directories(volk_android PUBLIC "${VENDOR_ROOT}/volk" "${VENDOR_ROOT}/Vulkan-Headers/Include")
target_compile_definitions(volk_android PUBLIC VK_USE_PLATFORM_ANDROID_KHR)

# SPIRV-Cross (minimal set needed by the shader compiler)
file(GLOB SPIRVCROSS_SOURCES "${VENDOR_ROOT}/SPIRV-Cross/Source/*.cpp")
add_library(spirvcross_android STATIC ${SPIRVCROSS_SOURCES})
target_include_directories(spirvcross_android PUBLIC "${VENDOR_ROOT}/SPIRV-Cross/Include/SPIRV-Cross")
target_compile_options(spirvcross_android PRIVATE
    -w
)

# ImGui
set(IMGUI_SOURCES
    "${VENDOR_ROOT}/ImGui/imgui.cpp"
    "${VENDOR_ROOT}/ImGui/imgui_demo.cpp"
    "${VENDOR_ROOT}/ImGui/imgui_draw.cpp"
    "${VENDOR_ROOT}/ImGui/imgui_tables.cpp"
    "${VENDOR_ROOT}/ImGui/imgui_widgets.cpp"
    "${VENDOR_ROOT}/ImGui/Backends/imgui_impl_vulkan.cpp"
    "${VENDOR_ROOT}/ImGui/Backends/imgui_impl_android.cpp"
    "${APP_GLUE_DIR}"
)
add_library(imgui_android STATIC ${IMGUI_SOURCES})
target_include_directories(imgui_android PUBLIC "${VENDOR_ROOT}/ImGui" "${VENDOR_ROOT}/volk" "${VENDOR_ROOT}/Vulkan-Headers/Include" "${VENDOR_ROOT}/glm")
target_compile_definitions(imgui_android PUBLIC VK_USE_PLATFORM_ANDROID_KHR IMGUI_IMPL_VULKAN_USE_VOLK)

# Link everything together
target_link_libraries(SurgePlayer PRIVATE
    volk_android
    spirvcross_android
    imgui_android
    ${log-lib}
    ${android-lib}
    ${vulkan-lib}
)