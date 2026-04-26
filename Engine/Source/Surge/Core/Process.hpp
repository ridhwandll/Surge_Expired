// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#ifdef SURGE_PLATFORM_WINDOWS
#include <string>

namespace Surge::Process
{
    int ResultOf(const std::wstring& commandLine);
    std::wstring OutputOf(const std::wstring& commandLine, int& result);
    std::wstring OutputOf(const std::wstring& commandLine);

} // namespace Surge::Process
#endif