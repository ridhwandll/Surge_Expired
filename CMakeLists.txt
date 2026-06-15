cmake_minimum_required(VERSION 3.20)
set(CMAKE_GENERATOR_TOOLSET "ClangCL")
project(Surge LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

#check the compiler
#check the compiler
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    # using Clang-CL (MSVC syntax)
    # /W4 (High warning level), /WX (Treat warnings as errors), /w44062 (Warn on unhandled switch enum)
    set(CMAKE_CXX_FLAGS "/W4 /fp:fast /GR- /MP /EHsc")
    add_compile_options("/showFilenames")
    MESSAGE(STATUS "[Surge] Compiler: ClangCL")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    # using Visual Studio C++
    set(CMAKE_CXX_FLAGS "/W4 /WX /fp:fast /GR- /MP /EHsc")
    MESSAGE(STATUS "[Surge] Compiler: MSVC")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    message(FATAL_ERROR "Use Clang/MSVC")
endif()

add_subdirectory(Engine)
add_subdirectory(Editor) #Disable editor for now, it is not ready yet
add_subdirectory(Player)
add_subdirectory(Engine/Vendor)