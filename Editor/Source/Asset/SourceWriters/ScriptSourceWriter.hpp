// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/String.hpp"

namespace Surge::ScriptSourceWriter
{
    String GetDefaultScriptContent();
    bool WriteNew(const String& absolutePath);
}
