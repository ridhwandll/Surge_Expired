// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/Path.hpp"
#include "Surge/Core/String.hpp"
#include <atomic>
#include <mutex>
#include <thread>

namespace Surge
{
    // Streams one app's device log into the Editor Console under LOG_TAG: the engine's own lines (SurgeEngine tag), warnings and errors
    // from the rest of its process, and its native crash report with the libSurgePlayer.so frames symbolized against the unstripped
    // library of the last Gradle build. Follows the app across restarts (new pid) and the device across replugging.
    // Own worker thread, the public API is main thread only
    class AndroidLogcat
    {
    public:
        static constexpr const char* LOG_TAG = "[Device]>>";

        struct Request
        {
            Path Adb;
            String PackageId;
            Path Symbolizer; // NDK llvm-symbolizer.exe, empty: crash frames stay raw
            Path SymbolsDir; // Searched for the newest unstripped libSurgePlayer.so (Android/app/build/intermediates/cxx)
        };

        enum class State
        {
            IDLE = 0,
            CONNECTING,
            STREAMING,
            DISCONNECTED, // adb lost the device, waiting for it to come back
            FAILED
        };

        AndroidLogcat() = default;
        ~AndroidLogcat() { Stop(); }
        SURGE_DISABLE_COPY_AND_MOVE(AndroidLogcat);

        void Start(Request request); // Restarts when already running
        void Stop();

        bool IsRunning() const { return mRunning; }
        String GetStatusText() const;

    private:
        void Run(); // Worker
        void Fail(const String& reason);

    private:
        Request mRequest;
        std::thread mWorker;
        std::atomic<bool> mStop { false };
        std::atomic<bool> mRunning { false };
        std::atomic<State> mState { State::IDLE };
        std::atomic<int> mAppPid { 0 };

        mutable std::mutex mErrorMutex;
        String mError;
    };

} // namespace Surge
