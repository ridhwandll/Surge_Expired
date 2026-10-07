// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Export/ExportSession.hpp"
#include "Export/ExportUtils.hpp"
#include "Export/ToolOutputParser.hpp"
#include "Surge/Core/Process.hpp"

#include <algorithm>
#include <filesystem>

namespace Surge
{
    using Clock = std::chrono::steady_clock;

    static float SecondsSince(Clock::time_point start)
    {
        return std::chrono::duration<float>(Clock::now() - start).count();
    }

    void ExportSession::Reset()
    {
        mState = ExportState::IDLE;
        mCancelRequested = false;
        mProgress = 0.0f;
        mDurationSeconds = 0.0f;
        mWarningCount = 0;
        mErrorCount = 0;
        mStepBegin = 0.0f;
        mStepEnd = 1.0f;

        std::scoped_lock lock(mStatusMutex);
        mStepName.clear();
        mStepDetail.clear();
        mFailureSummary.clear();
        mFailureIsSpecific = false;
        mResultWarning.clear();
        mArtifactPath.clear();
    }

    void ExportSession::Start(const String& firstStep)
    {
        mStartTime = Clock::now();
        {
            std::scoped_lock lock(mStatusMutex);
            mStepName = firstStep;
        }
        mState = ExportState::COOKING;
    }

    void ExportSession::RequestCancel()
    {
        if(!IsExportBusy(mState) || mCancelRequested)
            return;

        mCancelRequested = true;
        SetDetail("Cancelling...");
        Log(Severity::Warn, "Cancel requested");
    }

    ExportStatus ExportSession::GetStatus() const
    {
        ExportStatus status;
        status.State = mState;
        status.Progress = mProgress;
        status.ElapsedSeconds = IsExportBusy(status.State) ? SecondsSince(mStartTime) : mDurationSeconds.load();
        status.WarningCount = mWarningCount;
        status.ErrorCount = mErrorCount;
        status.CancelRequested = mCancelRequested;

        std::scoped_lock lock(mStatusMutex);
        status.StepName = mStepName;
        status.StepDetail = mStepDetail;
        status.FailureSummary = mFailureSummary;
        status.ResultWarning = mResultWarning;
        status.ArtifactPath = mArtifactPath;
        return status;
    }

    void ExportSession::Log(Severity severity, const String& text)
    {
        switch(severity)
        {
            case Severity::Trace: Surge::Log<Severity::Trace>("{} {}", LOG_TAG, text); break;
            case Severity::Info:  Surge::Log<Severity::Info>("{} {}", LOG_TAG, text); break;
            case Severity::Debug: Surge::Log<Severity::Debug>("{} {}", LOG_TAG, text); break;
            case Severity::Warn:
                mWarningCount++;
                Surge::Log<Severity::Warn>("{} {}", LOG_TAG, text);
                break;
            case Severity::Error:
            case Severity::Fatal:
                mErrorCount++;
                Surge::Log<Severity::Error>("{} {}", LOG_TAG, text);
                break;
        }
    }

    void ExportSession::Error(const String& text)
    {
        Log(Severity::Error, text);
        NoteFailure(text, false);
    }

    void ExportSession::NoteFailure(const String& text, bool specific)
    {
        std::scoped_lock lock(mStatusMutex);
        if(mFailureSummary.empty() || (specific && !mFailureIsSpecific))
        {
            mFailureSummary = text;
            mFailureIsSpecific = specific;
        }
    }

    void ExportSession::AdvanceProgress(float value)
    {
        // Only the worker writes while running, so a plain compare is enough
        if(value > mProgress)
            mProgress = std::min(value, 1.0f);
    }

    void ExportSession::SetStep(const String& name, float begin, float end)
    {
        {
            std::scoped_lock lock(mStatusMutex);
            mStepName = name;
            mStepDetail.clear();
        }
        mStepBegin = begin;
        mStepEnd = end;
        AdvanceProgress(begin);
        Log(Severity::Info, "> " + name);
    }

    void ExportSession::SetStepFraction(float fraction)
    {
        AdvanceProgress(mStepBegin + (mStepEnd - mStepBegin) * std::clamp(fraction, 0.0f, 1.0f));
    }

    void ExportSession::SetDetail(const String& detail)
    {
        std::scoped_lock lock(mStatusMutex);
        mStepDetail = detail;
    }

    void ExportSession::SetResult(const Path& artifact)
    {
        std::error_code ec;
        const uintmax_t size = std::filesystem::file_size(artifact, ec);
        Log(Severity::Info, "Output: " + artifact.string() + (ec ? String() : " (" + ExportUtils::FormatSize(size) + ")"));

        std::scoped_lock lock(mStatusMutex);
        mArtifactPath = artifact.string();
    }

    void ExportSession::SetResultWarning(const String& warning)
    {
        Log(Severity::Warn, warning);
        std::scoped_lock lock(mStatusMutex);
        mResultWarning = warning;
    }

    namespace
    {
        struct ToolRun
        {
            ExportSession* Session;
            ToolOutputParser* Parser;
            String Pending;
        };

        void OnToolOutput(const char* data, size_t size, void* userData)
        {
            ToolRun& run = *static_cast<ToolRun*>(userData);
            run.Pending.append(data, size);

            size_t start = 0;
            size_t newline;
            while((newline = run.Pending.find('\n', start)) != String::npos)
            {
                size_t end = newline;
                if(end > start && run.Pending[end - 1] == '\r')
                    --end;
                if(end > start)
                    run.Parser->OnLine(*run.Session, run.Pending.substr(start, end - start));
                start = newline + 1;
            }
            run.Pending.erase(0, start);
        }
    } // namespace

    int ExportSession::RunTool(const String& command, const Path& workDir, ToolOutputParser* parser)
    {
        ToolOutputParser defaultParser;
        ToolRun run { this, parser ? parser : &defaultParser, {} };
        run.Parser->Begin();

        const String dir = workDir.empty() ? String() : std::filesystem::absolute(workDir).string();
        const int exitCode = Process::Run(command, dir, &OnToolOutput, &run, &mCancelRequested);

        if(!run.Pending.empty() && run.Pending.back() == '\r')
            run.Pending.pop_back();
        if(!run.Pending.empty())
            run.Parser->OnLine(*this, run.Pending);

        return exitCode;
    }

    void ExportSession::Finish(bool succeeded)
    {
        const float seconds = SecondsSince(mStartTime);
        mDurationSeconds = seconds;

        if(!succeeded && mCancelRequested)
        {
            Log(Severity::Warn, "Export cancelled after " + ExportUtils::FormatDuration(seconds) + ".");
            mState = ExportState::CANCELLED;
            return;
        }

        if(succeeded)
        {
            mProgress = 1.0f;
            Log(Severity::Info, "Export finished in " + ExportUtils::FormatDuration(seconds) + ".");
        }
        else
            Log(Severity::Error, "Export FAILED after " + ExportUtils::FormatDuration(seconds) + ".");

        mState = succeeded ? ExportState::SUCCEEDED : ExportState::FAILED; // Last write, the main thread joins on it
    }

} // namespace Surge
