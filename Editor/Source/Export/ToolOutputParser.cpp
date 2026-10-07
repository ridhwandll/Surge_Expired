// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Export/ToolOutputParser.hpp"
#include "Export/ExportSession.hpp"
#include "Export/ExportUtils.hpp"

namespace Surge
{
    using ExportUtils::Contains;
    using ExportUtils::StartsWith;

    void ToolOutputParser::OnLine(ExportSession& session, const String& line)
    {
        Emit(session, Classify(line), line);
    }

    Severity ToolOutputParser::Classify(const String& line) const
    {
        // Prefix/pattern based: a file called ErrorHandler.cpp is not an error
        if(IsCompilerError(line) || StartsWith(line, "FAILED:") || StartsWith(line, "error:") || StartsWith(line, "Error:") || StartsWith(line, "ERROR:"))
            return Severity::Error;

        if(Contains(line, ": warning:") || StartsWith(line, "warning:") || StartsWith(line, "Warning:") || StartsWith(line, "WARNING:"))
            return Severity::Warn;

        return Severity::Trace;
    }

    void ToolOutputParser::Emit(ExportSession& session, Severity severity, const String& line) const
    {
        if(severity == Severity::Error || severity == Severity::Fatal)
            session.NoteFailure(line, IsCompilerError(line));
        session.Log(severity, line);
    }

    bool ToolOutputParser::IsCompilerError(const String& line)
    {
        return Contains(line, ": error:") || Contains(line, ": fatal error:");
    }

} // namespace Surge
