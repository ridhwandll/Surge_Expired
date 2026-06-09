// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Serializer/Serializer.hpp"
#include "Surge/ECS/Components.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Surge/Asset/Asset.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <json/json.hpp>

#ifdef SURGE_PLATFORM_ANDROID
#include "Surge/Platform/Android/AndroidApp.hpp"
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <android/asset_manager.h>
#else
#include <fstream>
#endif


// https://github.com/nlohmann/json#arbitrary-types-conversions
namespace glm
{
    inline void to_json(nlohmann::json& j, const vec2& p)
    {
        j = nlohmann::json {{"X", p.x}, {"Y", p.y}};
    }

    inline void from_json(const nlohmann::json& j, vec2& p)
    {
        j.at("X").get_to(p.x);
        j.at("Y").get_to(p.y);
    }

    inline void to_json(nlohmann::json& j, const vec3& p)
    {
        j = nlohmann::json {{"X", p.x}, {"Y", p.y}, {"Z", p.z}};
    }

    inline void from_json(const nlohmann::json& j, vec3& p)
    {
        j.at("X").get_to(p.x);
        j.at("Y").get_to(p.y);
        j.at("Z").get_to(p.z);
    }

    inline void to_json(nlohmann::json& j, const vec4& p)
    {
        j = nlohmann::json {{"X", p.x}, {"Y", p.y}, {"Z", p.z}, {"W", p.w}};
    }

    inline void from_json(const nlohmann::json& j, vec4& p)
    {
        j.at("X").get_to(p.x);
        j.at("Y").get_to(p.y);
        j.at("Z").get_to(p.z);
        j.at("W").get_to(p.w);
    }
} // namespace glm

namespace Surge
{

#pragma region SceneSerializer
    // Enum Mappings
    NLOHMANN_JSON_SERIALIZE_ENUM(RuntimeCamera::ProjectionType, { {RuntimeCamera::ProjectionType::Perspective, "Perspective"}, {RuntimeCamera::ProjectionType::Orthographic, "Orthographic"} });
    NLOHMANN_JSON_SERIALIZE_ENUM(LightType, { {LightType::POINT, "POINT"}, {LightType::DIRECTIONAL, "DIRECTIONAL"} });

    template <typename XComponent>
    FORCEINLINE static void SerializeComponent(nlohmann::json& j, Entity& e)
    {
        if (e.HasComponent<XComponent>())
        {
            XComponent& comp = e.GetComponent<XComponent>();
            const SurgeReflect::Class* clazz = SurgeReflect::GetReflection<XComponent>();

            nlohmann::json& out = j[clazz->GetName()];

            for (const auto& [name, var] : clazz->GetVariables())
            {
                uint64_t size = var.GetSize();
                const Byte* source = reinterpret_cast<const Byte*>(&comp) + var.GetOffset();

                const SurgeReflect::Type& type = var.GetType();
                if (type.EqualTo<bool>())
                {
                    out[name] = *reinterpret_cast<const bool*>(source);
                }
                else if (type.EqualTo<float>())
                {
                    out[name] = *reinterpret_cast<const float*>(source);
                }
                else if (type.EqualTo<UUID>() || type.EqualTo<AssetID>())
                {
                    out[name] = *reinterpret_cast<const uint64_t*>(source);
                }
                else if (type.EqualTo<String>())
                {
                    out[name] = *reinterpret_cast<const String*>(source);
                }
                else if (type.EqualTo<glm::vec3>())
                {
                    out[name] = *reinterpret_cast<const glm::vec3*>(source);
                }
                else if (type.EqualTo<RuntimeCamera>())
                {
                    nlohmann::json& camOut = out[name];
                    const RuntimeCamera* cam = reinterpret_cast<const RuntimeCamera*>(source);
                    camOut["Vertical FOV"] = cam->GetPerspectiveVerticalFOV();
                    camOut["Perspective NearClip"] = cam->GetPerspectiveNearClip();
                    camOut["Perspective FarClip"] = cam->GetPerspectiveFarClip();
                    camOut["Orthographic NearClip"] = cam->GetOrthographicNearClip();
                    camOut["Orthographic FarClip"] = cam->GetOrthographicFarClip();
                    camOut["Orthographic Size"] = cam->GetOrthographicSize();
                    camOut["Projection"] = cam->GetProjectionType();
                }
                else if(type.EqualTo<LightType>())
                {
                    out[name] = *reinterpret_cast<const LightType*>(source);
                }
                else
                    Log<Severity::Warn>("Unhandled Variable of type: '{0}' while serializing!", type.GetFullName());
            }
        }
    }

    template <typename... Components>
    FORCEINLINE void SerializeComponents(nlohmann::json& out, Entity& e)
    {
        (SerializeComponent<Components>(out, e), ...);
    }

    // nlohmann::json& j is in "Scene" scope
    static void SerializeEntity(nlohmann::json& j, Entity& e, uint64_t index)
    {
        SG_ASSERT_NOMSG(e);

        nlohmann::json& out = j[std::format("Entity{0}", index)];
        SerializeComponents<SERIALIZABLE_COMPONENTS>(out, e);
    }

    void Serializer::SerializeScene(const Path& path, Scene* in)
    {
#ifdef SURGE_PLATFORM_ANDROID
        Log<Severity::Error>("[Serializer] SerializeScene is unsupported on Android runtime. APK assets are readonly!");
        return;
#else
        SG_ASSERT_NOMSG(in);
        SCOPED_TIMER("Serialization");
        nlohmann::json outJson = nlohmann::json();

        uint64_t index = 0;
        in->GetRegistry().each([&](auto entityID) {
            Entity e = Entity(entityID, in);
            SerializeEntity(outJson["Scene"], e, index);
            index++;
        });

        const size_t size = in->GetRegistry().view<IDComponent>().size();
        outJson["Scene"]["Size"] = size;

        String result = outJson.dump(4);
        std::ofstream file(path.string(), std::ios::out | std::ios::trunc);
        if(!file.is_open())
        {
            Log<Severity::Error>("SerializeScene: Failed to open '{}'.", path.string().c_str());
            return;
        }

        file << result;
        file.close();
#endif
    }

    template <typename XComponent>
    static void DeserializeComponent(nlohmann::json& j, Entity& e)
    {
        const SurgeReflect::Class* clazz = SurgeReflect::GetReflection<XComponent>();

        if(!j.contains(clazz->GetName()))
            return;

        if(!e.HasComponent<XComponent>())
            e.AddComponent<XComponent>();

        nlohmann::json& inJson = j[clazz->GetName()];
        XComponent& comp = e.GetComponent<XComponent>();

        for(const auto& [name, var] : clazz->GetVariables())
        {
            const SurgeReflect::Type& type = var.GetType();
            Byte* dest = reinterpret_cast<Byte*>(&comp) + var.GetOffset();

            if(!inJson.contains(name))
            {
                Log<Severity::Warn>("DeserializeComponent: Missing field '{}', skipping", name);
                continue;
            }

            if(type.EqualTo<bool>())
                *reinterpret_cast<bool*>(dest) = inJson.value(name, false);
            else if(type.EqualTo<float>())
                *reinterpret_cast<float*>(dest) = inJson.value(name, 0.0f);
            else if(type.EqualTo<UUID>() || type.EqualTo<AssetID>())
                *reinterpret_cast<uint64_t*>(dest) = inJson.value(name, 0ULL);
            else if(type.EqualTo<String>())
                *reinterpret_cast<String*>(dest) = inJson.value(name, String());
            else if(type.EqualTo<glm::vec3>())
                *reinterpret_cast<glm::vec3*>(dest) = inJson.value(name, glm::vec3(0.0f));
            else if(type.EqualTo<RuntimeCamera>())
            {
                RuntimeCamera* cam = reinterpret_cast<RuntimeCamera*>(dest);
                nlohmann::json& camIn = inJson[name];
                cam->SetPerspectiveVerticalFOV(camIn["Vertical FOV"]);
                cam->SetPerspectiveNearClip(camIn["Perspective NearClip"]);
                cam->SetPerspectiveFarClip(camIn["Perspective FarClip"]);
                cam->SetOrthographicNearClip(camIn["Orthographic NearClip"]);
                cam->SetOrthographicFarClip(camIn["Orthographic FarClip"]);
                cam->SetOrthographicSize(camIn["Orthographic Size"]);
                cam->SetProjectionType(camIn["Projection"]);
            }
            else if(type.EqualTo<LightType>())
                *reinterpret_cast<LightType*>(dest) = inJson.value(name, LightType::DIRECTIONAL);
            else
                Log<Severity::Warn>("DeserializeComponent: Unhandled type '{}' for field '{}'", type.GetFullName(), name);
        }
    }

    template <typename... Components>
    FORCEINLINE void DeserializeComponents(nlohmann::json& json, Entity& e)
    {
        (DeserializeComponent<Components>(json, e), ...);
    }

    static void DeserializeEntity(nlohmann::json& j, Entity& e, uint64_t index)
    {
        SG_ASSERT_NOMSG(e);

        nlohmann::json& inJson = j[std::format("Entity{0}", index)];
        DeserializeComponents<SERIALIZABLE_COMPONENTS>(inJson, e);
    }

    void Serializer::DeserializeScene(const Path& path, Scene* out)
    {
        SG_ASSERT_NOMSG(out);
        auto& registry = out->GetRegistry();
        registry.clear();

        String jsonContents;

#ifdef SURGE_PLATFORM_ANDROID
        android_app* app = Android::GAndroidApp;
        AAssetManager* androidAssetMgr = app->activity->assetManager;

        // AAssetManager expects generic forward-slash paths
        AAsset* asset = AAssetManager_open(androidAssetMgr, path.generic_string().c_str(), AASSET_MODE_BUFFER);
        if(!asset)
        {
            Log<Severity::Error>("DeserializeScene: Failed to open '{}' via AAssetManager", path.string());
            return;
        }

        size_t size = AAsset_getLength(asset);
        jsonContents.resize(size, '\0');
        AAsset_read(asset, jsonContents.data(), size);
        AAsset_close(asset);
#else
        jsonContents = Filesystem::ReadFile<String>(path);
#endif

        nlohmann::json parsedJson = nlohmann::json::parse(jsonContents, nullptr, false);
        if(parsedJson.is_discarded())
        {
            Log<Severity::Error>("DeserializeScene: Corrupt or invalid JSON at '{}'", path.string());
            return;
        }

        uint64_t sceneSize = parsedJson["Scene"]["Size"];
        for(uint64_t i = 0; i < sceneSize; i++)
        {
            Entity newEntity;
            out->CreateEntity(newEntity, "");
            DeserializeEntity(parsedJson["Scene"], newEntity, i);
        }
    }
#pragma endregion

    void Serializer::SerializeProject(const Path& path, Project* in)
    {
#ifdef SURGE_PLATFORM_ANDROID
        Log<Severity::Error>("[Serializer] SerializeProject is unsupported on Android runtime. APK assets are readonly!");
        return;
#else
        SG_ASSERT_NOMSG(in);

        nlohmann::json outJson;
        outJson["Project"]["Name"] = in->Name;
        outJson["Project"]["Version"] = in->Version;
        outJson["Project"]["StartScene"] = static_cast<uint64_t>(in->StartScene);

        String result = outJson.dump(4);

        std::ofstream file(path.string(), std::ios::out | std::ios::trunc);
        if(!file.is_open())
        {
            Log<Severity::Error>("SerializeProject: Failed to open '{}'.", path.string().c_str());
            return;
        }

        file << result;
        file.close();
#endif
    }

    void Serializer::DeserializeProject(const Path& path, Project* out)
    {
        SG_ASSERT_NOMSG(out);

        String jsonContents;

#ifdef SURGE_PLATFORM_ANDROID
        android_app* app = Android::GAndroidApp;
        AAssetManager* androidAssetMgr = app->activity->assetManager;

        AAsset* asset = AAssetManager_open(androidAssetMgr, path.generic_string().c_str(), AASSET_MODE_BUFFER);
        if(!asset)
        {
            Log<Severity::Error>("DeserializeProject: Failed to open '{}' via AAssetManager", path.string());
            return;
        }

        size_t size = AAsset_getLength(asset);
        jsonContents.resize(size, '\0');
        AAsset_read(asset, jsonContents.data(), size);
        AAsset_close(asset);
#else
        jsonContents = Filesystem::ReadFile<String>(path);
#endif

        if(jsonContents.empty())
        {
            Log<Severity::Error>("DeserializeProject: File is empty or could not be read '{}'!", path.string());
            return;
        }

        try
        {
            nlohmann::json parsedJson = nlohmann::json::parse(jsonContents);

            if(parsedJson.contains("Project"))
            {
                const auto& projNode = parsedJson["Project"];
                out->Name = projNode.value("Name", "");
                out->Version = projNode.value("Version", "1.0");
                out->StartScene = static_cast<AssetID>(projNode.value("StartScene", 0ULL));
            }
        }
        catch(const nlohmann::json::exception& e)
        {
            Log<Severity::Error>("DeserializeProject: JSON parsing failed for '{}'. Error: {}", path.string(), e.what());
        }
    }

} // namespace Surge