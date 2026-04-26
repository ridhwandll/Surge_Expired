include(${CMAKE_SOURCE_DIR}/scripts/CMakeUtils.cmake)

file(GLOB_RECURSE SOURCE_FILES "Source/*.h" "Source/*.cpp" "Include/*.h")
set(INCLUDE_DIRS Include)

add_library(fmt STATIC ${SOURCE_FILES})
include_directories(${INCLUDE_DIRS})

GroupSourcesByFolder(fmt)
set_target_properties(fmt PROPERTIES FOLDER Dependencies)
