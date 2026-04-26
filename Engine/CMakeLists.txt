include(${CMAKE_SOURCE_DIR}/scripts/CMakeUtils.cmake)

file(GLOB_RECURSE SOURCE_FILES
    Source/*.cpp
    Source/*.hpp

    Vendor/stb/stb_image.cpp
    Vendor/cgltf/cgltf.cpp
)

set(INCLUDE_DIRS
    Source
    Source/SurgeReflect/Include
    Vendor
    Vendor/fmt/Include
    Vendor/glm
    Vendor/Vulkan-Headers/Include
    Vendor/VulkanMemoryAllocator/Include
    Vendor/volk
    Vendor/shaderc/Include
    Vendor/SPIRV-Cross/Include
    Vendor/stb
    Vendor/ImGui
    Vendor/Optick/Include
    Vendor/entt/Include
    Vendor/json/Include
    Vendor/FontAwesome
    Vendor/cgltf
)

set(LIB_LINKS
    fmt
    volk
    SPIRV-Cross
    ImGui
    #Optick
    ${CMAKE_SOURCE_DIR}/Engine/Vendor/shaderc/Lib/shaderc_shared.lib
)

add_library(Surge STATIC ${SOURCE_FILES})
target_include_directories(Surge PUBLIC ${INCLUDE_DIRS})
target_link_libraries(Surge PUBLIC ${LIB_LINKS})

# Add Precompiled Header
target_precompile_headers(Surge PRIVATE "Source/Surge/Pch.hpp")

GroupSourcesByFolder(Surge)

set_property(TARGET Surge PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_target_properties(Surge PROPERTIES FOLDER Engine)

if (WIN32)
    set(PLATFORM_COMPILE_DEFS VK_USE_PLATFORM_WIN32_KHR NOMINMAX SURGE_PLATFORM_WINDOWS)
endif (WIN32)
if (ANDROID)
    set(PLATFORM_COMPILE_DEFS VK_USE_PLATFORM_ANDROID_KHR SURGE_PLATFORM_ANDROID)
endif (ANDROID)


target_compile_definitions(Surge

    PUBLIC
    "_CRT_SECURE_NO_WARNINGS"
    "_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING"
    "GLM_FORCE_DEPTH_ZERO_TO_ONE"
    "GLM_FORCE_RADIANS"
    ${PLATFORM_COMPILE_DEFS}

    $<$<CONFIG:Debug>:SURGE_DEBUG>
    $<$<CONFIG:Release>:SURGE_RELEASE>

    PRIVATE
    SURGE_EXPORT
)

add_custom_command(TARGET Surge PRE_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/Editor/$<CONFIGURATION>/Libraries)
CopyBinaryToExeDir(Surge ${CMAKE_BINARY_DIR}/Engine/$<CONFIGURATION>/Surge.lib Editor)
CopyBinaryToExeDir(Surge ${CMAKE_BINARY_DIR}/Engine/Vendor/ImGui/$<CONFIGURATION>/ImGui.lib Editor)
CopyBinaryToExeDir(Surge ${CMAKE_BINARY_DIR}/Engine/Vendor/fmt/$<CONFIGURATION>/fmt.lib Editor)
#CopyBinaryToExeDir(Surge ${CMAKE_BINARY_DIR}/Engine/Vendor/Optick/$<CONFIGURATION>/Optick.lib Editor)
CopyBinaryToExeDir(Surge ${CMAKE_BINARY_DIR}/Engine/Vendor/SPIRV-Cross/$<CONFIGURATION>/SPIRV-Cross.lib Editor)
CopyBinaryToExeDir(Surge ${CMAKE_BINARY_DIR}/Engine/Vendor/volk/$<CONFIGURATION>/volk.lib Editor)
CopyBinaryToExeDir(Surge ${CMAKE_SOURCE_DIR}/Engine/Vendor/shaderc/Lib/shaderc_shared.lib Editor)

add_custom_command(
    TARGET Surge
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:Surge>"
        "$<TARGET_FILE_DIR:Editor>"
    COMMENT "Build dll: Copying Surge dll to Editor output directory"
    VERBATIM
)