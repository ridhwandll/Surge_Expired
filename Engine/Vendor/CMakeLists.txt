function(DisableTargetWarnings TARGET_NAME)
    if(MSVC)
        target_compile_options(${TARGET_NAME} PRIVATE /W0)
    else()
        # GCC and Clang use -w (lowercase) to disable all warnings
        target_compile_options(${TARGET_NAME} PRIVATE -w)
    endif()
endfunction()

add_subdirectory(volk)
add_subdirectory(SPIRV-Cross)
add_subdirectory(ImGui)
#add_subdirectory(Optick)

DisableTargetWarnings(volk)
DisableTargetWarnings(SPIRV-Cross)
DisableTargetWarnings(ImGui)