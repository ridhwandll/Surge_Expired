// Copyright (c) - SurgeTechnologies - All rights reserved
#include "SceneBinding.hpp"
#include "Surge/ScriptEngine/Lua.hpp"
#include "Surge/ScriptEngine/ScriptEngine.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Core/Client.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/ECS/Scene.hpp"
#include "Surge/Physics/Physics.hpp"

namespace Surge::ScriptBinding
{
    static AssetID sPendingScene = AssetID(AssetID::INVALID);

    // The scene scripts run in (owned by the client)
    static Scene* RuntimeScene()
    {
        Client* client = Core::GetClient();
        if(!client)
            return nullptr;
        Ref<Scene> scene = client->GetRuntimeScene(); // The client keeps its own reference, the pointer stays valid
        return scene ? scene.Raw() : nullptr;
    }

    static sol::object EntityOrNil(sol::this_state s, Entity entity)
    {
        return entity ? sol::make_object(s, entity) : sol::make_object(s, sol::lua_nil);
    }

    void BindScene(void* luaState)
    {
        sol::state_view& lua = *static_cast<sol::state_view*>(luaState);

        // Time: scaled by Time.SetTimeScale (physics and script delta time), "Unscaled" variants follow the real clock
        sol::table time = lua.create_named_table("Time");
        time["GetDeltaTime"] = []() { return Core::GetScriptEngine()->GetDeltaTime(); };
        time["GetUnscaledDeltaTime"] = []() { return Core::GetScriptEngine()->GetUnscaledDeltaTime(); };
        time["GetTime"] = []() { return Core::GetScriptEngine()->GetTime(); };
        time["GetUnscaledTime"] = []() { return Core::GetScriptEngine()->GetUnscaledTime(); };
        time["GetFrameCount"] = []() { return static_cast<lua_Integer>(Core::GetScriptEngine()->GetFrameCount()); };
        time["GetFixedDeltaTime"] = []() { return Core::GetPhysics()->GetFixedTimestep(); };
        time["GetTimeScale"] = &Core::GetTimeScale;
        time["SetTimeScale"] = &Core::SetTimeScale;

        // Scene: the running scene
        sol::table scene = lua.create_named_table("Scene");
        scene["FindEntity"] = [](const String& name, sol::this_state s) -> sol::object {
            Scene* runtime = RuntimeScene();
            return runtime ? EntityOrNil(s, runtime->GetEntityByName(name)) : sol::make_object(s, sol::lua_nil);
        };
        scene["CreateEntity"] = [](const String& name, sol::this_state s) -> sol::object {
            Entity entity;
            if(Scene* runtime = RuntimeScene())
                runtime->CreateEntity(entity, name);
            return EntityOrNil(s, entity);
        };
        // Switches to a fresh copy of the scene asset at the end of the frame. Modules and _G survive: they are the session's
        // state, not the scene's. Several calls in one frame: the last one wins
        scene["Load"] = [](const String& scenePath) -> bool {
            AssetManager* am = Core::GetAssetManager();
            const AssetID id = am->GetIDFromPath(scenePath);
            if(!id.IsValid() || am->GetMetadata(id).Type != AssetType::SCENE)
            {
                Log<Severity::Warn>("[ScriptEngine] Scene.Load: '{}' is not a scene asset (paths are relative to the Assets folder)", scenePath);
                return false;
            }

            const bool alreadyScheduled = sPendingScene.IsValid();
            sPendingScene = id;
            if(!alreadyScheduled)
            {
                Core::AddFrameEndCallback([]() {
                    const AssetID target = sPendingScene;
                    sPendingScene = AssetID(AssetID::INVALID);
                    Core::GetClient()->ChangeRuntimeScene(target);
                });
            }
            return true;
        };

        // Physics queries
        sol::table physics = lua.create_named_table("Physics");
        physics["Raycast"] = [](const glm::vec3& origin, const glm::vec3& direction, float maxDistance, sol::optional<lua_Integer> layerMask, sol::this_state s) -> sol::object {
            RaycastHit hit;
            if(!Core::GetPhysics()->Raycast(origin, direction, maxDistance, hit, static_cast<uint32_t>(layerMask.value_or(0xFFFFFFFF))))
                return sol::make_object(s, sol::lua_nil);

            sol::state_view view(s);
            sol::table result = view.create_table(0, 4);
            Scene* runtime = RuntimeScene();
            const entt::entity handle = static_cast<entt::entity>(hit.EnttID);
            if(runtime && runtime->GetRegistry().valid(handle))
                result["Entity"] = Entity(handle, runtime);
            result["Point"] = hit.Point;
            result["Normal"] = hit.Normal;
            result["Distance"] = hit.Distance;
            return result;
        };

        physics["GetLayer"] = [](const String& name) -> lua_Integer { return Core::GetPhysics()->GetSettings().FindLayer(name); };
        physics["GetLayerName"] = [](int layer) -> String {
            return layer >= 0 && layer < PhysicsSettings::MAX_LAYERS ? Core::GetPhysics()->GetSettings().LayerNames[layer] : String();
        };
        physics["GetLayerMask"] = [](sol::variadic_args names) -> lua_Integer {
            const PhysicsSettings& settings = Core::GetPhysics()->GetSettings();
            lua_Integer mask = 0;
            for(const sol::stack_proxy name : names)
            {
                const int layer = name.is<String>() ? settings.FindLayer(name.as<String>()) : -1;
                if(layer < 0)
                    Log<Severity::Warn>("[Physics] GetLayerMask: unknown layer '{}'", name.is<String>() ? name.as<String>() : String("?"));
                else
                    mask |= lua_Integer(1) << layer;
            }
            return mask;
        };
        physics["ShouldCollide"] = [](int layerA, int layerB) {
            return Core::GetPhysics()->GetSettings().ShouldCollide(PhysicsSettings::ClampLayer(layerA), PhysicsSettings::ClampLayer(layerB));
        };
    }
}
