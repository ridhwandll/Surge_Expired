cmake_minimum_required(VERSION 3.20)
set(CMAKE_GENERATOR_TOOLSET "ClangCL")
project(Surge LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

#check the compiler
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    # using Clang-CL
    # Here the /MP flag is MSVC only, remove it in real Clang builds
    string(APPEND CMAKE_CXX_FLAGS " -g3 -w -m64 -mfma -ffast-math -fno-rtti /MP")
    add_compile_options("/showFilenames")
    MESSAGE(STATUS "[Surge] Compiler: Clang")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    # using Visual Studio C++
    set(CMAKE_CXX_FLAGS "/W2 /fp:fast /GR- /MP /EHsc")
    MESSAGE(STATUS "[Surge] Compiler: MSVC")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")    
    message(FATAL_ERROR "Use Clang/MSVC")
endif()

add_subdirectory(Engine)
add_subdirectory(Editor)
add_subdirectory(Player)
add_subdirectory(Engine/Vendor)