// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include <chrono>
#include <string>
#include "Surge/Core/Logger/Logger.hpp"

namespace Surge
{
    class Timer
    {
    public:
        Timer(const std::string& name = "Timer", bool logOnDestructor = false) : mName(name), mLogOnDestructor(logOnDestructor) { Reset(); }

        ~Timer()
        {
            if (mLogOnDestructor)
                Log<Severity::Trace>("{0} took {1} seconds({2} ms)!", mName, Elapsed(), ElapsedMillis());
        }

        void Reset() { mStart = std::chrono::high_resolution_clock::now(); }

        // Returns the elapsed time in seconds
        float Elapsed() { return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - mStart).count() * 0.001f * 0.001f * 0.001f; }

        float ElapsedMillis() { return Elapsed() * 1000.0f; }

    private:
        std::string mName;
        bool mLogOnDestructor;
        std::chrono::time_point<std::chrono::high_resolution_clock> mStart;
    };

} // namespace Surge
