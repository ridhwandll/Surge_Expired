// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/String.hpp"

namespace Surge::Process
{
    int ResultOf(const String& commandLine);
    String OutputOf(const String& commandLine, int& result);
    String OutputOf(const String& commandLine);

} // namespace Surge::Process
