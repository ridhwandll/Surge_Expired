// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/Memory.hpp"
#include "Surge/Core/Logger/Logger.hpp"
#include "Surge/Core/Time/Timer.hpp"
#include "Surge/Core/String.hpp"
#include "Surge/Core/Path.hpp"
#include "Surge/Core/Profiler.hpp"
#include "Surge/Utility/Platform.hpp"

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <array>
#include <deque>

#ifdef SURGE_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#elif SURGE_PLATFORM_ANDROID
#include <android/log.h>
#endif