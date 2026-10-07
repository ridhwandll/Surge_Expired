// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/String.hpp"
#include <deque>
#include <mutex>

namespace Surge
{
    struct ConsoleMessage
    {
        uint64_t ID = 0;   // Monotonic, consecutive IDs live in consecutive slots
        Severity Level = Severity::Trace;
        String Text;
        char Time[9] = {}; // HH:MM:SS of the latest occurrence
        Uint Count = 1;    // Identical consecutive messages are collapsed into one entry
    };

    // Receives every Log() line through the Logger sink
    class EditorConsole
    {
    public:
        static constexpr size_t MAX_MESSAGES = 4069;

        static EditorConsole& Get();
        static void Install();
        static void Uninstall();

        void Clear();

        // Hold the lock while reading the messages, Log() can come from any thread
        std::mutex& GetMutex() { return mMutex; }
        const std::deque<ConsoleMessage>& GetMessages() const { return mMessages; }

        uint64_t GetRevision() const { return mRevision; } // Bumped on every change
        Uint GetCount(Severity severity) const { return mCounts[static_cast<int>(severity)]; } // Since the last Clear()
        Uint GetWarningCount() const { return GetCount(Severity::Warn); }
        Uint GetErrorCount() const { return GetCount(Severity::Error) + GetCount(Severity::Fatal); }

    private:
        static void OnLog(Severity severity, const String& message, const std::tm& localTime, void* userData);

    private:
        std::mutex mMutex;
        std::deque<ConsoleMessage> mMessages;
        uint64_t mNextID = 1;
        uint64_t mRevision = 0;
        Uint mCounts[static_cast<int>(Severity::Fatal) + 1] = {};
    };

} // namespace Surge
