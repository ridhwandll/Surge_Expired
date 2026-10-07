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

    // Screen space UI canvas
    // Layout     : UILayout asset (.sui, made in the UI Editor), instantiated when the scene starts running
    // ScriptAsset: optional Lua script driving the canvas, find layout widgets with entity.UICanvasC:FindWidget("Name")
    //              or build widgets in code and call SetUIRoot(root) (attaches to this canvas)
    // SortOrder  : canvases with a higher sort order draw on top and receive input first
    struct UICanvasComponent
    {
        bool Active = true;
        bool ShowCanvas = true;
        Ref<Asset> Layout = nullptr;
        int SortOrder = 0;
        Ref<Asset> ScriptAsset = nullptr;
        bool IsInstantiated = false;
        uint64_t RuntimeCanvasID = 0; // Entity UUID, key of this canvas in UI::Manager while the scene runs (0 = not registered)

        // ScriptData: (Maybe abstract these sol into some wrapper in future?)
        sol::environment Env;
        sol::protected_function OnCreate;
        sol::protected_function OnUpdate;
        sol::protected_function OnDestroy;
        sol::protected_function OnCollisionEnter;

        SURGE_REFLECTION_ENABLE;
    };

} // namespace Surge
