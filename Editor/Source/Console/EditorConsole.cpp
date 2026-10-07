// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Console/EditorConsole.hpp"
#include <algorithm>
#include <cstdio>

namespace Surge
{
    EditorConsole& EditorConsole::Get()
    {
        static EditorConsole sInstance;
        return sInstance;
    }

    void EditorConsole::Install()
    {
        SetLogSink(&EditorConsole::OnLog, &Get());
    }

    void EditorConsole::Uninstall()
    {
        SetLogSink(nullptr, nullptr);
    }

    void EditorConsole::Clear()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mMessages.clear();
        std::fill(std::begin(mCounts), std::end(mCounts), 0u);
        mRevision++;
    }

    void EditorConsole::OnLog(Severity severity, const String& message, const std::tm& localTime, void* userData)
    {
        EditorConsole* console = static_cast<EditorConsole*>(userData);
        std::lock_guard<std::mutex> lock(console->mMutex);

        console->mCounts[static_cast<int>(severity)]++;
        console->mRevision++;

        ConsoleMessage* target = nullptr;

        // Per-frame spam collapses into one row with a counter
        if(!console->mMessages.empty())
        {
            ConsoleMessage& last = console->mMessages.back();
            if(last.Level == severity && last.Text == message)
            {
                last.Count++;
                target = &last;
            }
        }

        if(!target)
        {
            if(console->mMessages.size() >= MAX_MESSAGES)
                console->mMessages.pop_front();

            ConsoleMessage& entry = console->mMessages.emplace_back();
            entry.ID = console->mNextID++;
            entry.Level = severity;
            entry.Text = message;
            target = &entry;
        }

        std::snprintf(target->Time, sizeof(target->Time), "%02d:%02d:%02d", localTime.tm_hour, localTime.tm_min, localTime.tm_sec);
    }

} // namespace Surge
