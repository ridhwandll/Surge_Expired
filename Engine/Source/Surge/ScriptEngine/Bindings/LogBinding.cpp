// Copyright (c) - SurgeTechnologies - All rights reserved
#include "LogBinding.hpp"
#include "Surge/Core/String.hpp"
#include "Surge/Core/Logger/Logger.hpp"
#include "Surge/ScriptEngine/Lua.hpp"

namespace Surge::ScriptBinding
{
    void BindLog(void* luaState)
    {
        sol::state_view* lua = static_cast<sol::state_view*>(luaState);

        auto log = lua->create_table("Log");
        log["Trace"] = [](const String& m) { Log<Severity::Trace>("Lua: {}", m); };
        log["Info"] = [](const String& m) { Log<Severity::Info>("Lua: {}", m); };
        log["Debug"] = [](const String& m) { Log<Severity::Debug>("Lua: {}", m); };
        log["Warn"] = [](const String& m) { Log<Severity::Warn>("Lua: {}", m); };
        log["Error"] = [](const String& m) { Log<Severity::Error>("Lua: {}", m); };
    }
}

