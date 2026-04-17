add_library(volk STATIC volk.c volk.h)
target_include_directories(volk PRIVATE ${CMAKE_SOURCE_DIR}/Engine/Vendor/Vulkan-Headers/Include)
set_target_properties(volk PROPERTIES FOLDER Dependencies)

if (WIN32)
    target_compile_definitions(volk PUBLIC VK_USE_PLATFORM_WIN32_KHR)
endif()
