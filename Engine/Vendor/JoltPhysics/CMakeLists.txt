file(GLOB_RECURSE JOLT_SOURCES "Jolt/*.cpp" "Jolt/*.h" "Jolt/*.hpp")
add_library(Jolt STATIC ${JOLT_SOURCES})
target_include_directories(Jolt PUBLIC "")
target_compile_definitions(Jolt PUBLIC JPH_DEBUG_RENDERER)

set_target_properties(Jolt PROPERTIES FOLDER Dependencies)
if(WIN32)

    target_compile_definitions(Jolt PUBLIC 
        JPH_USE_AVX2 
        JPH_USE_SSE4_1 
        JPH_USE_SSE4_2 
        JPH_USE_LZCNT 
        JPH_USE_TZCNT 
        JPH_USE_F16C 
        JPH_USE_FMADD
    )
    target_compile_options(Jolt PUBLIC /arch:AVX2 /fp:fast)
elseif(ANDROID)
    target_compile_options(Jolt PUBLIC -ffast-math)
endif()
