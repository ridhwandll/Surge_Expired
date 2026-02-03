cmake_minimum_required(VERSION 3.20)

project(Surge LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

#check the compiler
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    # using Clang
    set(CMAKE_CXX_FLAGS "-g3 -O0 -Wall -pedantic -Wextra -m64 -mfma -ffast-math -fno-rtti")
    MESSAGE(STATUS "[Surge] Compiler id ${CMAKE_CXX_COMPILER_ID}")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    # using GCC
    set(CMAKE_CXX_FLAGS "-Wall -pedantic -Wextra -m64 -mfma -ffast-math -fno-rtti")
    MESSAGE(STATUS "[Surge] Compiler id ${CMAKE_CXX_COMPILER_ID}")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    # using Visual Studio C++
    set(CMAKE_CXX_FLAGS "/W3 /fp:fast /GR- /MP /EHsc /wd4251") #C4251: Bad DLL warning
    MESSAGE(STATUS "[Surge] Compiler id ${CMAKE_CXX_COMPILER_ID}(The best compiler)")
endif()

add_subdirectory(Engine)
add_subdirectory(Editor)
add_subdirectory(Engine/Vendor)