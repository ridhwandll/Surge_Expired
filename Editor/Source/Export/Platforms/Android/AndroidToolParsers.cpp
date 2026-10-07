// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Export/Platforms/Android/AndroidToolParsers.hpp"
#include "Export/ExportSession.hpp"
#include "Export/ExportUtils.hpp"

#include <algorithm>
#include <cstdlib>

namespace Surge
{
    using ExportUtils::Contains;
    using ExportUtils::StartsWith;

    namespace
    {
        constexpr float NATIVE_BEGIN = 0.20f; // Share of the Gradle step before / after the native build
        constexpr float NATIVE_END = 0.75f;

        // "[12/345] Building CXX object CMakeFiles/Surge.dir/.../VulkanRHI.cpp.o"
        bool ParseNinjaProgress(const String& line, int& done, int& total, String& action)
        {
            if(line.size() < 5 || line[0] != '[')
                return false;

            char* end = nullptr;
            const char* doneBegin = line.c_str() + 1;
            done = static_cast<int>(std::strtol(doneBegin, &end, 10));
            if(end == doneBegin || *end != '/')
                return false;

            const char* totalBegin = end + 1;
            total = static_cast<int>(std::strtol(totalBegin, &end, 10));
            if(end == totalBegin || *end != ']' || total <= 0)
                return false;

            action = String(end + 1);
            if(!action.empty() && action.front() == ' ')
                action.erase(0, 1);
            return true;
        }

        String DescribeNinjaAction(const String& action)
        {
            const size_t slash = action.find_last_of("/\\ ");
            String file = slash == String::npos ? action : action.substr(slash + 1);
            if(file.ends_with(".o"))
                file.resize(file.size() - 2);

            if(StartsWith(action, "Building"))
                return "COMPILING " + file;
            if(StartsWith(action, "Linking"))
                return "LINKING " + file;
            return action;
        }
    } // namespace

    void GradleOutputParser::Begin()
    {
        mFraction = 0.0f;
        mPhase = NativePhase::BEFORE;
        mCaptureNextAsFailure = false;
    }

    void GradleOutputParser::OnLine(ExportSession& session, const String& line)
    {
        String text = line;
        if(StartsWith(text, "C/C++: ")) // AGP prefixes everything the native build prints
            text.erase(0, 7);
        if(text.empty())
            return;

        int done = 0, total = 0;
        String action;
        if(ParseNinjaProgress(text, done, total, action))
        {
            if(mPhase == NativePhase::BEFORE)
                mPhase = NativePhase::COMPILING;
            mFraction = std::max(mFraction, NATIVE_BEGIN + (NATIVE_END - NATIVE_BEGIN) * static_cast<float>(done) / static_cast<float>(total));
            session.SetStepFraction(mFraction);
            session.SetDetail(std::format("{} ({}/{})", DescribeNinjaAction(action), done, total));
            if(mVerbose)
                session.Log(Severity::Trace, text);
            return;
        }

        if(StartsWith(text, "> Task "))
        {
            const String task = text.substr(7);
            if(Contains(task, "CMake") || Contains(task, "NativeBuild"))
            {
                if(mPhase == NativePhase::BEFORE)
                    mPhase = NativePhase::COMPILING;
            }
            else if(mPhase == NativePhase::COMPILING)
            {
                mPhase = NativePhase::DONE;
                mFraction = std::max(mFraction, NATIVE_END);
            }

            // Each task closes 8% of the gap to the phase's cap: tasks are many and of unknown cost
            const float cap = mPhase == NativePhase::BEFORE ? NATIVE_BEGIN : (mPhase == NativePhase::DONE ? 1.0f : mFraction);
            mFraction += (cap - mFraction) * 0.08f;
            session.SetStepFraction(mFraction);
            session.SetDetail(task);
            session.Log(Severity::Trace, text);
            return;
        }

        if(StartsWith(text, "ninja: no work to do"))
        {
            mPhase = NativePhase::DONE;
            mFraction = std::max(mFraction, NATIVE_END);
            session.SetStepFraction(mFraction);
        }

        if(mCaptureNextAsFailure)
        {
            mCaptureNextAsFailure = false;
            Emit(session, Severity::Error, text);
            return;
        }
        if(StartsWith(text, "* What went wrong:"))
        {
            mCaptureNextAsFailure = true;
            session.Log(Severity::Error, text);
            return;
        }

        const Severity severity = Classify(text);
        if(StartsWith(text, "FAILURE:") || StartsWith(text, "BUILD FAILED")) // Headers, not the reason
            session.Log(severity, text);
        else
            Emit(session, severity, text);
    }

    Severity GradleOutputParser::Classify(const String& line) const
    {
        if(StartsWith(line, "FAILURE:") || StartsWith(line, "BUILD FAILED") || StartsWith(line, "e: "))
            return Severity::Error;
        if(StartsWith(line, "w: "))
            return Severity::Warn;
        if(StartsWith(line, "BUILD SUCCESSFUL"))
            return Severity::Info;
        return ToolOutputParser::Classify(line);
    }

    Severity AdbOutputParser::Classify(const String& line) const
    {
        if(StartsWith(line, "adb: failed") || Contains(line, "INSTALL_FAILED") || StartsWith(line, "** No activities found"))
            return Severity::Error;
        if(StartsWith(line, "Success"))
            return Severity::Info;
        return ToolOutputParser::Classify(line);
    }

} // namespace Surge
