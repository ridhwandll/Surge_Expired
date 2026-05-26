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

    private:
        std::chrono::high_resolution_clock::time_point mStart;
        std::chrono::high_resolution_clock::time_point mThen;
        std::chrono::high_resolution_clock::time_point mNow;
    };
} // namespace Surge
