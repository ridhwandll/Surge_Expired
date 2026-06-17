// Copyright (c) - SurgeTechnologies - All rights reserved
#include "ECSBindings.hpp"
#include "Surge/ScriptEngine/Lua.hpp"

namespace Surge::ScriptBinding
{
    void BindEntity(void* luaState)
    {
        sol::state_view* lua = static_cast<sol::state_view*>(luaState);
    }

    void BindComponents(void* luaState)
    {
        sol::state_view* lua = static_cast<sol::state_view*>(luaState);
    }
}
