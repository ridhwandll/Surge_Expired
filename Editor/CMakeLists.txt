include(${CMAKE_SOURCE_DIR}/scripts/CMakeUtils.cmake)

set(INCLUDE_DIRS Source)
file(GLOB_RECURSE SOURCE_FILES Source/*.cpp Source/*.hpp)

add_executable(Editor ${SOURCE_FILES})
target_link_libraries(Editor PRIVATE Surge)
target_include_directories(Editor PRIVATE ${INCLUDE_DIRS})

GroupSourcesByFolder(Editor)
set_property(TARGET Editor PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
set_target_properties(Editor PROPERTIES FOLDER App)