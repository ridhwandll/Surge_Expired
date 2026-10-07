// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/String.hpp"

namespace Surge
{
    class ExportSession;

    // Turns an external tool's output (one line at a time, on the worker thread) into Console logs, failure notes and progress.
    // The default handles compiler-style diagnostics; derive for tools whose output carries more (Gradle tasks, Ninja's [n/N])
    class ToolOutputParser
    {
    public:
        virtual ~ToolOutputParser() = default;

        virtual void Begin() {} // Before every run
        virtual void OnLine(ExportSession& session, const String& line);

    protected:
        virtual Severity Classify(const String& line) const;

        // Logs the line, and remembers error lines as the failure summary
        void Emit(ExportSession& session, Severity severity, const String& line) const;

        static bool IsCompilerError(const String& line); // "file:line:col: error: ..."
    };

} // namespace Surge
