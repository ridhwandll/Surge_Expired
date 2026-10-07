// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/String.hpp"
#include <atomic>

namespace Surge::Process
{
    // Receives raw chunks of the child's combined stdout/stderr (not split into lines), on the thread that called Run()
    using OutputCallback = void (*)(const char* data, size_t size, void* userData);

    int ResultOf(const String& commandLine);
    String OutputOf(const String& commandLine, int& result);
    String OutputOf(const String& commandLine);

    // Runs commandLine through the system shell in workDir (empty = current), streaming its output while it runs.
    // Setting *cancel terminates the whole process tree. Returns the exit code, -1 if it could not start or was cancelled
    int Run(const String& commandLine, const String& workDir, OutputCallback onOutput, void* userData, const std::atomic<bool>* cancel = nullptr);

} // namespace Surge::Process
