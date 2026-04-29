include(${CMAKE_SOURCE_DIR}/scripts/CMakeUtils.cmake)

set(INCLUDE_DIRS Source)
file(GLOB_RECURSE SOURCE_FILES Source/*.cpp Source/*.hpp)

add_executable(Player ${SOURCE_FILES})
target_link_libraries(Player PRIVATE Surge)
target_include_directories(Player PRIVATE ${INCLUDE_DIRS})

GroupSourcesByFolder(Player)
set_property(TARGET Player PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
set_target_properties(Player PROPERTIES FOLDER App)