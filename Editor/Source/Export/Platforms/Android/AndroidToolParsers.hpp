// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Export/ToolOutputParser.hpp"

namespace Surge
{
    // Gradle (--console=plain) with AGP's native build output (android.native.buildOutput=build_stdout).
    // Progress: tasks creep towards the native build, Ninja's [n/N] drives it, the remaining tasks creep to the end
    class GradleOutputParser : public ToolOutputParser
    {
    public:
        explicit GradleOutputParser(bool verbose)
            : mVerbose(verbose) {}

        virtual void Begin() override;
        virtual void OnLine(ExportSession& session, const String& line) override;

    protected:
        virtual Severity Classify(const String& line) const override;

    private:
        enum class NativePhase { BEFORE, COMPILING, DONE };

        bool mVerbose;                 // false: Ninja's per-file lines only update the detail text
        float mFraction = 0.0f;        // Of the Gradle step
        NativePhase mPhase = NativePhase::BEFORE;
        bool mCaptureNextAsFailure = false; // The line after "* What went wrong:" is the actual reason
    };

    class AdbOutputParser : public ToolOutputParser
    {
    protected:
        virtual Severity Classify(const String& line) const override;
    };

} // namespace Surge
