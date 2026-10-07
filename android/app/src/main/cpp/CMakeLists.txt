# Copyright (c) - SurgeTechnologies - All rights reserved
# Android build of Surge: Engine + Player = libSurgePlayer.so (driven by android/app/build.gradle)
# This file does NOT use Engine/CMakeLists.txt. When adding a vendor lib, include dir or compile define,
# keep it in sync with the desktop CMake files

cmake_minimum_required(VERSION 3.21)
project(SurgePlayer LANGUAGES C CXX)

if (NOT ANDROID)
    message(FATAL_ERROR "[Surge] This CMakeLists.txt only targets Android. Use Scripts/WindowsGenProjects.bat for Windows; Apple is not supported YET.")
endif()

# Global settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON) # Every static lib ends up inside a shared object

option(SURGE_ANDROID_USE_PCH "Compile the engine with its precompiled header (Surge/Pch.hpp)" ON)

# Paths
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE) # 5 dirs behind

set(ENGINE_ROOT     "${REPO_ROOT}/Engine")
set(ENGINE_SRC_ROOT "${ENGINE_ROOT}/Source")
set(VENDOR_ROOT     "${ENGINE_ROOT}/Vendor")
set(PLAYER_SRC_ROOT "${REPO_ROOT}/Player/Source")

# Helpers
# SurgeAddVendorLibrary(<name>
#     SOURCES          <files...>
#     [PUBLIC_INCLUDES <dirs...>]  [PRIVATE_INCLUDES <dirs...>]
#     [PUBLIC_DEFINES  <defs...>]  [PRIVATE_DEFINES  <defs...>]
#     [PUBLIC_OPTIONS  <opts...>]
#     [LINKS           <libs...>])
# Builds a third-party static library with its warnings silenced (vendor code is never modified).
function(SurgeAddVendorLibrary name)
    cmake_parse_arguments(PARSE_ARGV 1 ARG ""
        ""
        "SOURCES;PUBLIC_INCLUDES;PRIVATE_INCLUDES;PUBLIC_DEFINES;PRIVATE_DEFINES;PUBLIC_OPTIONS;LINKS")

    if (NOT ARG_SOURCES)
        message(FATAL_ERROR "[Surge] Vendor library '${name}' has no sources")
    endif()

    add_library(${name} STATIC ${ARG_SOURCES})
    target_compile_options(${name} PRIVATE -w)

    if (ARG_PUBLIC_INCLUDES)
        target_include_directories(${name} SYSTEM PUBLIC ${ARG_PUBLIC_INCLUDES})
    endif()
    if (ARG_PRIVATE_INCLUDES)
        target_include_directories(${name} PRIVATE ${ARG_PRIVATE_INCLUDES})
    endif()
    if (ARG_PUBLIC_DEFINES)
        target_compile_definitions(${name} PUBLIC ${ARG_PUBLIC_DEFINES})
    endif()
    if (ARG_PRIVATE_DEFINES)
        target_compile_definitions(${name} PRIVATE ${ARG_PRIVATE_DEFINES})
    endif()
    if (ARG_PUBLIC_OPTIONS)
        target_compile_options(${name} PUBLIC ${ARG_PUBLIC_OPTIONS})
    endif()
    if (ARG_LINKS)
        target_link_libraries(${name} PRIVATE ${ARG_LINKS})
    endif()
endfunction()

# External packages / NDK system libraries
find_package(game-activity REQUIRED CONFIG) # GameActivity (provided via Prefab)

set(ANDROID_SYSTEM_LIBS android log vulkan)

# Vendor libraries
set(VULKAN_HEADERS_INCLUDE "${VENDOR_ROOT}/Vulkan-Headers/Include")

# volk
SurgeAddVendorLibrary(volk
    SOURCES         "${VENDOR_ROOT}/volk/volk.c"
    PUBLIC_INCLUDES "${VENDOR_ROOT}/volk" "${VULKAN_HEADERS_INCLUDE}"
    PUBLIC_DEFINES  VK_USE_PLATFORM_ANDROID_KHR
)

# SPIRV-Cross
file(GLOB SPIRV_CROSS_SOURCES CONFIGURE_DEPENDS "${VENDOR_ROOT}/SPIRV-Cross/Source/*.cpp")
SurgeAddVendorLibrary(SPIRV-Cross
    SOURCES         ${SPIRV_CROSS_SOURCES}
    PUBLIC_INCLUDES "${VENDOR_ROOT}/SPIRV-Cross/Include/SPIRV-Cross"
)

# ImGui (+ ImGuizmo, Vulkan/Android backends)
set(IMGUI_ROOT "${VENDOR_ROOT}/ImGui")
SurgeAddVendorLibrary(ImGui
    SOURCES
        "${IMGUI_ROOT}/imgui.cpp"
        "${IMGUI_ROOT}/imgui_demo.cpp"
        "${IMGUI_ROOT}/imgui_draw.cpp"
        "${IMGUI_ROOT}/imgui_tables.cpp"
        "${IMGUI_ROOT}/imgui_widgets.cpp"
        "${IMGUI_ROOT}/imgui_stdlib.cpp"
        "${IMGUI_ROOT}/ImGuizmo.cpp"
        "${IMGUI_ROOT}/Backends/imgui_impl_vulkan.cpp"
        "${IMGUI_ROOT}/Backends/imgui_impl_android.cpp"
    PUBLIC_INCLUDES "${IMGUI_ROOT}" "${VENDOR_ROOT}/glm"
    PUBLIC_DEFINES  IMGUI_IMPL_VULKAN_USE_VOLK
    LINKS           volk ${ANDROID_SYSTEM_LIBS}
)

# basisu transcoder (KTX2 + zstd)
SurgeAddVendorLibrary(basisu
    SOURCES
        "${VENDOR_ROOT}/basisu/transcoder/basisu_transcoder.cpp"
        "${VENDOR_ROOT}/basisu/zstd/zstd.c"
        "${VENDOR_ROOT}/basisu/zstd/zstddeclib.c"
    PUBLIC_INCLUDES "${VENDOR_ROOT}/basisu/transcoder"
    PRIVATE_DEFINES
        BASISD_SUPPORT_KTX2_ZSTD=1
        BASISD_SUPPORT_PVRTC1=0
        BASISD_SUPPORT_PVRTC2=0
        BASISD_SUPPORT_ATC=0
        BASISD_SUPPORT_FXT1=0
)

# JoltPhysics
file(GLOB_RECURSE JOLT_SOURCES CONFIGURE_DEPENDS "${VENDOR_ROOT}/JoltPhysics/Jolt/*.cpp")
SurgeAddVendorLibrary(Jolt
    SOURCES         ${JOLT_SOURCES}
    PUBLIC_INCLUDES "${VENDOR_ROOT}/JoltPhysics"
    PUBLIC_DEFINES  JPH_DEBUG_RENDERER
    PUBLIC_OPTIONS  -ffast-math
)

# lua
file(GLOB LUA_SOURCES CONFIGURE_DEPENDS "${VENDOR_ROOT}/lua/src/*.c")
SurgeAddVendorLibrary(lua
    SOURCES         ${LUA_SOURCES}
    PUBLIC_INCLUDES "${VENDOR_ROOT}/lua/src"
)

# miniaudio
SurgeAddVendorLibrary(miniaudio
    SOURCES "${VENDOR_ROOT}/miniaudio/miniaudio.c"
)

set(SURGE_VENDOR_LIBS volk SPIRV-Cross ImGui basisu Jolt lua miniaudio)

# Engine (OBJECT library: objects are linked straight into the .so
# So, self-registering translation units are never dropped by the linker
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
    "${ENGINE_SRC_ROOT}/*.cpp"
    "${ENGINE_SRC_ROOT}/*.hpp"
)
list(FILTER ENGINE_SOURCES EXCLUDE REGEX ".*/Platform/Windows/.*")

add_library(Surge OBJECT ${ENGINE_SOURCES})

target_include_directories(Surge PUBLIC
    "${ENGINE_SRC_ROOT}"
    "${ENGINE_SRC_ROOT}/SurgeReflect/Include"
)

# Header-only vendors (sol2, nlohmann json, entt, glm, VMA, Optick macros) + include roots of the form <vendor/header>
target_include_directories(Surge SYSTEM PUBLIC
    "${ENGINE_ROOT}"
    "${VENDOR_ROOT}"
    "${VENDOR_ROOT}/glm"
    "${VULKAN_HEADERS_INCLUDE}"
    "${VENDOR_ROOT}/VulkanMemoryAllocator/Include"
    "${VENDOR_ROOT}/SPIRV-Cross/Include"
    "${VENDOR_ROOT}/Optick/Include"
    "${VENDOR_ROOT}/entt/Include"
    "${VENDOR_ROOT}/json/Include"
)

target_compile_definitions(Surge
    PUBLIC
        SURGE_PLATFORM_ANDROID
        VK_USE_PLATFORM_ANDROID_KHR
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_FORCE_RADIANS
        _CRT_SECURE_NO_WARNINGS
        $<$<CONFIG:Debug>:SURGE_DEBUG>
        $<$<CONFIG:MinSizeRel>:SURGE_DEBUG>
        $<$<CONFIG:RelWithDebInfo>:SURGE_DEBUG>
        $<$<CONFIG:Release>:SURGE_RELEASE>
    PRIVATE
        SURGE_EXPORT
)

target_link_libraries(Surge PUBLIC
    game-activity::game-activity_static
    ${SURGE_VENDOR_LIBS}
    ${ANDROID_SYSTEM_LIBS}
)

if (SURGE_ANDROID_USE_PCH)
    target_precompile_headers(Surge PRIVATE "${ENGINE_SRC_ROOT}/Surge/Pch.hpp")
endif()

# Player (the shared library packaged into the APK)
file(GLOB_RECURSE PLAYER_SOURCES CONFIGURE_DEPENDS
    "${PLAYER_SRC_ROOT}/*.cpp"
    "${PLAYER_SRC_ROOT}/*.hpp"
)
list(FILTER PLAYER_SOURCES EXCLUDE REGEX ".*/WindowsMain\\.cpp$")

add_library(SurgePlayer SHARED ${PLAYER_SOURCES})
target_include_directories(SurgePlayer PRIVATE "${PLAYER_SRC_ROOT}")
target_link_libraries(SurgePlayer PRIVATE Surge)

target_link_options(SurgePlayer PRIVATE
    "-Wl,-z,max-page-size=16384"              # 16 KB page-size devices (Android 15+)
    "$<$<CONFIG:Release>:-Wl,--gc-sections>"  # Strip unreferenced code/data from the shipped .so
)
