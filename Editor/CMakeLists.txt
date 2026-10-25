include(${CMAKE_SOURCE_DIR}/scripts/CMakeUtils.cmake)

set(INCLUDE_DIRS
    Source
    Vendor/stb
    Vendor/cgltf
    Vendor/meshoptimizer/src
)
    
file(GLOB_RECURSE SOURCE_FILES
    Source/*.cpp
    Source/*.hpp

    Vendor/stb/stb_image.cpp
    Vendor/cgltf/cgltf.cpp)


add_executable(Editor ${SOURCE_FILES})

# Exe icon (Explorer, taskbar), the project only enables C/CXX so the resource compiler is enabled here
enable_language(RC)
target_sources(Editor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Assets/Resources/Editor.rc)
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/Assets/Resources/Editor.rc PROPERTIES OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Assets/Resources/Editor.ico)

# The shader compiler
target_link_libraries(Editor PRIVATE Surge meshoptimizer SPIRV-Cross ${CMAKE_SOURCE_DIR}/Engine/Vendor/shaderc/Lib/shaderc_shared.lib)
target_include_directories(Editor PRIVATE ${INCLUDE_DIRS})
target_include_directories(Editor SYSTEM PRIVATE
    ${CMAKE_SOURCE_DIR}/Engine/Vendor/shaderc/Include
    ${CMAKE_SOURCE_DIR}/Engine/Vendor/SPIRV-Cross/Include
)

GroupSourcesByFolder(Editor)
set_property(TARGET Editor PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
set_target_properties(Editor PROPERTIES FOLDER App)

add_custom_command(TARGET Editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/Engine/Vendor/shaderc/Binaries/shaderc_shared.dll"
        $<TARGET_FILE_DIR:Editor>
    COMMENT "Copying shaderc_shared.dll to Editor.exe directory..."
)

add_subdirectory(Vendor/meshoptimizer)