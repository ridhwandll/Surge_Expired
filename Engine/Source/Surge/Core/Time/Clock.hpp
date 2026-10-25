// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include <chrono>

namespace Surge
{
    class Clock
    {
    public:
        // Starts the timer. Initializes Life and Delta to current time
        void Start();

        // Returns the time (in seconds) since the engine was initialized
        float GetLife();

        // Returns the delta time since the last frame
        float GetSeconds();
        float GetMilliseconds();

        // Updates the timer. Increments Life and refreshes Delta
        void Update();

        // Zeroes the next delta, used after the app was suspended so the first frame back does not see the whole pause as dt
        void ResetDelta();

        // Game time scale (0 = frozen, 1 = normal), Core::SetTimeScale: scales the physics step accumulation and the delta time scripts get
        void SetTimeScale(float scale) { mTimeScale = scale > 0.0f ? scale : 0.0f; }
        float GetTimeScale() const { return mTimeScale; }

    private:
        std::chrono::high_resolution_clock::time_point mStart;
        std::chrono::high_resolution_clock::time_point mThen;
        std::chrono::high_resolution_clock::time_point mNow;
        float mTimeScale = 1.0f;
    };

} // namespace Surge
