include(${CMAKE_SOURCE_DIR}/scripts/CMakeUtils.cmake)

set(INCLUDE_DIRS Source)
file(GLOB_RECURSE SOURCE_FILES Source/*.cpp Source/*.hpp)

add_executable(Editor ${SOURCE_FILES})
target_link_libraries(Editor PRIVATE Surge)
target_include_directories(Editor PRIVATE ${INCLUDE_DIRS})

# Copy the dlls to the bin directory
add_custom_command(
    TARGET Editor
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/Engine/Vendor/shaderc/Binaries/shaderc_shared.dll
        ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIGURATION>
    )

add_custom_command(
    TARGET Editor
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:Surge>"
        "$<TARGET_FILE_DIR:Editor>"
    COMMENT "Copying Surge DLL to Editor output directory"
    VERBATIM
)

GroupSourcesByFolder(Editor)
set_property(TARGET Editor PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
set_target_properties(Editor PROPERTIES FOLDER App)