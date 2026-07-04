// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once

#ifdef SURGE_PLATFORM_ANDROID

    // Optick does not support Android; disable profiling unconditionally
    #define PROFILE_SURGE 0
    #define USE_OPTICK 0

#elif defined(SURGE_PLATFORM_WINDOWS)

    #ifdef SURGE_DEBUG
    #define PROFILE_SURGE 1
    #define USE_OPTICK 1
    #include <optick.h>
    #endif

// Enable profiling for release builds if SURGE_RELEASE is defined
#define PROFILE_RELEASE 0

#ifdef SURGE_RELEASE
    #ifdef PROFILE_RELEASE
        #define PROFILE_SURGE 1
        #define USE_OPTICK 1
        #include <optick.h>
    #else
        #define PROFILE_SURGE 0
        #define USE_OPTICK 0
    #endif
#endif

#endif

#if PROFILE_SURGE
#define SURGE_PROFILE_FRAME(...) OPTICK_FRAME(__VA_ARGS__)
#define SURGE_PROFILE_FUNC(...) OPTICK_EVENT(__VA_ARGS__)
#define SURGE_PROFILE_TAG(NAME, ...) OPTICK_TAG(NAME, __VA_ARGS__)
#define SURGE_PROFILE_THREAD(...) OPTICK_THREAD(__VA_ARGS__)
#else
#define SURGE_PROFILE_FRAME(...)
#define SURGE_PROFILE_FUNC(...)
#define SURGE_PROFILE_TAG(NAME, ...)
#define SURGE_PROFILE_THREAD(...)
#endif