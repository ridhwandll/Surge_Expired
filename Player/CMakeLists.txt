include(${CMAKE_SOURCE_DIR}/scripts/CMakeUtils.cmake)

set(INCLUDE_DIRS Source)
file(GLOB_RECURSE SOURCE_FILES Source/*.cpp Source/*.hpp)

add_executable(Player ${SOURCE_FILES})
target_link_libraries(Player PRIVATE Surge)
target_include_directories(Player PRIVATE ${INCLUDE_DIRS})

# Copy the dlls to the bin directory
add_custom_command(
    TARGET Player
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/Engine/Vendor/shaderc/Binaries/shaderc_shared.dll
        ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIGURATION>
    )
add_custom_command(
    TARGET Player
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/Engine/Vendor/assimp/Binaries/assimp-vc142-mt.dll
        ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIGURATION>
    )

add_custom_command(
    TARGET Player
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:Surge>"
        "$<TARGET_FILE_DIR:Editor>"
    COMMENT "Copying Surge DLL to Editor output directory"
    VERBATIM
)

GroupSourcesByFolder(Player)
set_property(TARGET Player PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
set_target_properties(Player PROPERTIES FOLDER App)