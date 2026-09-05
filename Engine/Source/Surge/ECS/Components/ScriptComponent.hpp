// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Asset/Asset.hpp"
#include "SurgeReflect/SurgeReflect.hpp"
#include "Surge/ScriptEngine/Lua.hpp"

namespace Surge
{
    struct ScriptComponent
    {
        bool Active = true;
        Ref<Asset> ScriptAsset = nullptr;
        bool IsInstantiated = false;

        // ScriptData: (Maybe abstract these sol into some wrapper in future?)
        sol::environment Env;
        sol::protected_function OnCreate;
        sol::protected_function OnUpdate;
        sol::protected_function OnDestroy;
        sol::protected_function OnCollisionEnter;

        SURGE_REFLECTION_ENABLE;
    };

    // TODO: Make this a normal script in future, do not have UICanvasComponent maybe?
    struct UICanvasComponent
    {
        bool Active = true;
        bool ShowCanvas = true;
        Ref<Asset> ScriptAsset = nullptr;
        bool IsInstantiated = false;

        // ScriptData: (Maybe abstract these sol into some wrapper in future?)
        sol::environment Env;
        sol::protected_function OnCreate;
        sol::protected_function OnUpdate;
        sol::protected_function OnDestroy;
        sol::protected_function OnCollisionEnter;

        SURGE_REFLECTION_ENABLE;
    };

} // namespace Surge
