
file(GLOB_RECURSE LUA_SOURCES
    src/*.c
    src/*.h
)

add_library(lua STATIC ${LUA_SOURCES}
#    src/lapi.c
#    src/ldo.c
#    src/lvm.c
#    src/lstate.c
#    src/lmem.c
#    src/lgc.c
#    src/lfunc.c
#    src/ltable.c
#    src/lobject.c
#    src/lstring.c
#    src/ltm.c
#    src/lctype.c
#    src/lzio.c
#
#    # Script loading pipeline
#    src/llex.c
#    src/lparser.c
#    src/lcode.c
#    src/lundump.c
#    src/lopcodes.c
#    src/linit.c
#
#    # Auxiliary + debug
#    src/lauxlib.c
#    src/ldebug.c
#    src/ldblib.c
#
#    # Standard libraries
#    src/lbaselib.c
#    src/lmathlib.c
#    src/lstrlib.c
#    src/ltablib.c
#    src/lcorolib.c
#    src/lutf8lib.c
)

target_include_directories(lua PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(lua PROPERTIES FOLDER Dependencies)
target_compile_definitions(lua PRIVATE
    LUA_NOENV
)

#if(ANDROID)
#    target_compile_definitions(lua PRIVATE LUA_USE_ANDROID)
#    target_compile_options(lua PRIVATE
#        $<$<CONFIG:Release>:-O3 -ffast-math -march=armv8-a>
#        $<$<CONFIG:Debug>:-O0 -g>
#    )
target_compile_definitions(lua PRIVATE _CRT_SECURE_NO_WARNINGS)
