// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/String.hpp"
#include "Surge/ScriptEngine/ScriptAsset.hpp"

namespace Surge::ScriptSourceWriter
{
    String GetDefaultScriptContent();
    bool WriteNew(const String& absolutePath);
}
