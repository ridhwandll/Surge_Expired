// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once

#ifdef SURGE_PLATFORM_ANDROID
// Optick does not support Android; disable profiling unconditionally
#define PROFILE_SURGE 0
#define USE_OPTICK 0
#else

#ifdef SURGE_DEBUG
#define PROFILE_SURGE 1
#define USE_OPTICK 1
#endif

#ifdef SURGE_RELEASE
#define PROFILE_SURGE 0
#define USE_OPTICK 0 // Enable if required in release mode
#endif

//#include <optick.h>
#endif // SURGE_PLATFORM_ANDROID

#if PROFILE_SURGE
#define SURGE_PROFILE_FRAME(...) //OPTICK_FRAME(__VA_ARGS__)
#define SURGE_PROFILE_FUNC(...) //OPTICK_EVENT(__VA_ARGS__)
#define SURGE_PROFILE_TAG(NAME, ...) //OPTICK_TAG(NAME, __VA_ARGS__)
#define SURGE_PROFILE_THREAD(...) //OPTICK_THREAD(__VA_ARGS__)
#else
#define SURGE_PROFILE_FRAME(...)
#define SURGE_PROFILE_FUNC(...)
#define SURGE_PROFILE_TAG(NAME, ...)
#define SURGE_PROFILE_THREAD(...)
#endif