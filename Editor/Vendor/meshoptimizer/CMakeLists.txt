include(${CMAKE_SOURCE_DIR}/scripts/CMakeUtils.cmake)

file(GLOB_RECURSE SOURCE_FILES "src/*.cpp" "src/*.h")
set(INCLUDE_DIRS src)

add_library(meshoptimizer ${SOURCE_FILES})
include_directories(${INCLUDE_DIRS})

GroupSourcesByFolder(meshoptimizer)
set_target_properties(meshoptimizer PROPERTIES FOLDER Dependencies)
