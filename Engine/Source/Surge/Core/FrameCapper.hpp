// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include <chrono>
#include <thread>

namespace Surge
{
    class FrameCapper
    {
    public:
        FrameCapper(float targetFPS = 60.0f)
        {
#ifndef SURGE_PLATFORM_ANDROID
            SetTargetFPS(targetFPS);
#endif
        }

        void SetTargetFPS(float fps)
        {
#ifndef SURGE_PLATFORM_ANDROID
            if(fps > 0.0f)
                mTargetFrameTime = std::chrono::duration<double, std::milli>(1000.0 / fps);
            else
                mTargetFrameTime = std::chrono::duration<double, std::milli>(0.0);
#endif
        }

        void BeginFrame()
        {
#ifndef SURGE_PLATFORM_ANDROID
            mFrameStartTime = std::chrono::high_resolution_clock::now();
#endif
        }

        void EndFrameAndCap()
        {
#ifndef SURGE_PLATFORM_ANDROID
            if(mTargetFrameTime.count() <= 0.0)
                return; // Uncapped

            auto frameEndTime = std::chrono::high_resolution_clock::now();
            auto timeTaken = frameEndTime - mFrameStartTime;
            auto timeToSleep = mTargetFrameTime - timeTaken;

            if(timeToSleep.count() > 0.0)
            {
                // Sleep for the bulk of the time, yielding back to the OS
                // Wake up 2ms early to avoid Windows scheduler oversleeping
                auto sleepDuration = timeToSleep - std::chrono::duration<double, std::milli>(2.0);
                if(sleepDuration.count() > 0.0)
                    std::this_thread::sleep_for(sleepDuration);

                // Spin-lock (busy wait) for the final fraction of a millisecond for absolute precision
                while(true)
                {
                    auto currentTime = std::chrono::high_resolution_clock::now();
                    auto totalTime = currentTime - mFrameStartTime;
                    if(totalTime >= mTargetFrameTime)
                        break;
                }
            }
#endif
        }
#ifndef SURGE_PLATFORM_ANDROID
    private:
        std::chrono::time_point<std::chrono::high_resolution_clock> mFrameStartTime;
        std::chrono::duration<double, std::milli> mTargetFrameTime;
#endif
    };
}