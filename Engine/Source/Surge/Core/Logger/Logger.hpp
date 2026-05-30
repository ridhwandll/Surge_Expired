// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/String.hpp"
#include <stdio.h>
#include <mutex>
#include <format>

#ifdef SURGE_PLATFORM_ANDROID
#include <android/log.h>
#define SURGE_TAG "SurgeEngine"
#elif defined(SURGE_PLATFORM_WINDOWS)
//#define WIN32_LEAN_AND_MEAN
//#include <windows.h>
#endif

namespace Surge
{
	namespace LogColor {
		constexpr const char* Reset = "\x1b[0m";
		constexpr const char* Red = "\x1b[31m";
		constexpr const char* Green = "\x1b[32m";
		constexpr const char* Yellow = "\x1b[33m";
		constexpr const char* Blue = "\x1b[34m";
		constexpr const char* Cyan = "\x1b[36m";
		constexpr const char* Bold = "\x1b[1m";
        constexpr const char* Fatal = "\x1b[41;97m";
	}

    enum class Severity
    {
        Trace = 0,
        Info,
        Debug,
        Warn,
        Error,
        Fatal
    };

    static std::mutex sLogMutex;

	template <Severity severity = Severity::Trace, typename... Args>
	void Log(const std::string& fmtMsg, Args&&... args)
    {
        std::lock_guard<std::mutex> lock(sLogMutex);

        std::time_t now = std::time(nullptr);
        std::tm ltm{};
#ifdef _WIN32
        localtime_s(&ltm, &now);
#else
        localtime_r(&now, &ltm);
#endif

        // Format message
        std::string message = std::vformat(fmtMsg, std::make_format_args(args...));
        String finalMsg = std::format("[{:02}:{:02}:{:02}] {}", ltm.tm_hour, ltm.tm_min, ltm.tm_sec, message);

        // Output
#ifdef SURGE_PLATFORM_ANDROID
        switch (severity)
        {
        case Severity::Trace:
        case Severity::Info:  __android_log_print(ANDROID_LOG_INFO, SURGE_TAG, "%s", finalMsg.c_str()); break;
        case Severity::Debug: __android_log_print(ANDROID_LOG_DEBUG, SURGE_TAG, "%s", finalMsg.c_str()); break;
        case Severity::Warn:  __android_log_print(ANDROID_LOG_WARN, SURGE_TAG, "%s", finalMsg.c_str()); break;
        case Severity::Error: __android_log_print(ANDROID_LOG_ERROR, SURGE_TAG, "%s", finalMsg.c_str()); break;
        case Severity::Fatal: __android_log_print(ANDROID_LOG_FATAL, SURGE_TAG, "%s", finalMsg.c_str()); break;
        }
#elif defined(SURGE_PLATFORM_WINDOWS)

        const char* color = nullptr;
		switch (severity)
		{
        case Severity::Trace: color = LogColor::Reset; break;
		case Severity::Info:  color = LogColor::Green; break;
		case Severity::Warn:  color = LogColor::Yellow; break;
		case Severity::Error: color = LogColor::Red; break;
		case Severity::Debug: color = LogColor::Cyan; break;
		case Severity::Fatal: color = LogColor::Fatal; break;
		default: color = LogColor::Reset; break;
		}

		std::fwrite(color, 1, strlen(color), stdout);
		std::fwrite(finalMsg.data(), 1, finalMsg.size(), stdout);
		std::fwrite(LogColor::Reset, 1, strlen(LogColor::Reset), stdout);
		std::fputc('\n', stdout);
#endif
    }
} // namespace Surge
