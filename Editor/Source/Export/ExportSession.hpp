// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Export/ExportTypes.hpp"
#include <atomic>
#include <chrono>
#include <mutex>

namespace Surge
{
    class ToolOutputParser;

    // State of one export run, shared by the main thread (UI, cancel) and the worker (steps, logs, results). Thread-safe.
    class ExportSession
    {
    public:
        static constexpr const char* LOG_TAG = "[Export]>>";

        ExportSession() = default;
        SURGE_DISABLE_COPY_AND_MOVE(ExportSession);

        // Main thread
        void Reset();
        void Start(const String& firstStep); // Starts the clock, enters COOKING
        void RequestCancel();
        void SetState(ExportState state) { mState = state; }
        ExportState GetState() const { return mState; }
        ExportStatus GetStatus() const;

        // Worker (or main thread before the worker starts)
        void Log(Severity severity, const String& text);
        void Error(const String& text); // Logs it and keeps it as the failure summary if there is none yet
        void NoteFailure(const String& text, bool specific); // specific: a real diagnostic (compiler error) beats a generic one ("Gradle failed")

        // Each step owns [begin, end] of the progress bar, SetStepFraction() fills it in
        void SetStep(const String& name, float begin, float end);
        void SetStepFraction(float fraction);
        void SetDetail(const String& detail);

        void SetResult(const Path& artifact);
        void SetResultWarning(const String& warning);

        bool IsCancelled() const { return mCancelRequested; }

        // Runs a shell command, streams its output through the parser (default: ToolOutputParser) while it runs.
        // Returns the exit code, -1 if it could not start or was cancelled
        int RunTool(const String& command, const Path& workDir, ToolOutputParser* parser = nullptr);

        // Last call of a run: logs the outcome, enters SUCCEEDED / FAILED / CANCELLED
        void Finish(bool succeeded);

    private:
        void AdvanceProgress(float value);

    private:
        std::atomic<ExportState> mState { ExportState::IDLE };
        std::atomic<bool> mCancelRequested { false };
        std::atomic<float> mProgress { 0.0f };
        std::atomic<float> mDurationSeconds { 0.0f };
        std::atomic<Uint> mWarningCount { 0 };
        std::atomic<Uint> mErrorCount { 0 };
        std::chrono::steady_clock::time_point mStartTime;

        // Written by one step at a time (worker)
        float mStepBegin = 0.0f;
        float mStepEnd = 1.0f;

        mutable std::mutex mStatusMutex; // Guards the strings below
        String mStepName;
        String mStepDetail;
        String mFailureSummary;
        String mResultWarning;
        String mArtifactPath;
        bool mFailureIsSpecific = false;
    };

} // namespace Surge
