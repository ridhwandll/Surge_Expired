// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Serializer/Serializer.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Asset/Asset.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/ECS/Scene.hpp"
#include "Surge/ECS/Components.hpp"
#include "Surge/ScriptEngine/ScriptAsset.hpp"
#include "Surge/Graphics/HighLevel/Font.hpp"
#include "Surge/Graphics/HighLevel/Mesh.hpp"
#include "Surge/Graphics/HighLevel/Texture2D.hpp"
#include "Surge/Utility/Filesystem.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <json/json.hpp>

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
    NLOHMANN_JSON_SERIALIZE_ENUM(RigidbodyType, { {RigidbodyType::STATIC, "STATIC"}, {RigidbodyType::DYNAMIC, "DYNAMIC"}, {RigidbodyType::KINEMATIC, "KINEMATIC"} });
    NLOHMANN_JSON_SERIALIZE_ENUM(TextAlignment, { {TextAlignment::LEFT, "LEFT"}, {TextAlignment::CENTER, "CENTER"}, {TextAlignment::RIGHT, "RIGHT"} });
    NLOHMANN_JSON_SERIALIZE_ENUM(TextVerticalAlignment, { {TextVerticalAlignment::TOP, "TOP"}, {TextVerticalAlignment::CENTER, "CENTER"}, {TextVerticalAlignment::BOTTOM, "BOTTOM"}, {TextVerticalAlignment::BASELINE, "BASELINE"} });

    template <typename XComponent>
    static void SerializeComponent(nlohmann::json& j, Entity& e)
    {
        if (e.HasComponent<XComponent>())
        {
            XComponent& comp = e.GetComponent<XComponent>();
            const SurgeReflect::Class* clazz = SurgeReflect::GetReflection<XComponent>();

            nlohmann::json& out = j[clazz->GetName()];

            // Handle RelationshipComponent separately
            if constexpr (std::is_same_v<XComponent, RelationshipComponent>)
            {
                auto GetUUID = [&](entt::entity ent) -> uint64_t {
                    if(ent == entt::null)
                        return UUID::INVALID;
                    return Entity(ent, e.GetScene()).GetComponent<IDComponent>().ID;
                };

                out["Parent"] = GetUUID((entt::entity)comp.Parent);
                out["FirstChild"] = GetUUID((entt::entity)comp.FirstChild);
                out["PreviousSibling"] = GetUUID((entt::entity)comp.PreviousSibling);
                out["NextSibling"] = GetUUID((entt::entity)comp.NextSibling);
                out["ChildrenCount"] = comp.ChildrenCount;
                return;
            }

            for (const auto& [name, var] : clazz->GetVariables())
            {
                const Byte* source = reinterpret_cast<const Byte*>(&comp) + var.GetOffset();

                const SurgeReflect::Type& type = var.GetType();
                if (type.EqualTo<bool>())
                    out[name] = *reinterpret_cast<const bool*>(source);
                else if (type.EqualTo<float>())
                    out[name] = *reinterpret_cast<const float*>(source);
                else if (type.EqualTo<UUID>() || type.EqualTo<AssetID>())
                    out[name] = *reinterpret_cast<const uint64_t*>(source);
                else if (type.EqualTo<String>())
                    out[name] = *reinterpret_cast<const String*>(source);
                else if (type.EqualTo<glm::vec2>())
                    out[name] = *reinterpret_cast<const glm::vec2*>(source);
                else if (type.EqualTo<glm::vec3>())
                    out[name] = *reinterpret_cast<const glm::vec3*>(source);
                else if (type.EqualTo<glm::vec4>())
                    out[name] = *reinterpret_cast<const glm::vec4*>(source);
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
                    out[name] = *reinterpret_cast<const LightType*>(source);
                else if(type.EqualTo<RigidbodyType>())
                    out[name] = *reinterpret_cast<const RigidbodyType*>(source);
                else if(type.EqualTo<TextAlignment>())
                    out[name] = *reinterpret_cast<const TextAlignment*>(source);
                else if(type.EqualTo<TextVerticalAlignment>())
                    out[name] = *reinterpret_cast<const TextVerticalAlignment*>(source);
                else if(type.EqualTo<Ref<Asset>>())
                {
                    const Ref<Asset>& asset = *reinterpret_cast<const Ref<Asset>*>(source);
                    if(asset)
                        out[name] = asset->GetID().Get();
                    else
                        out[name] = AssetID::INVALID;
                }
                else
                    Log<Severity::Warn>("Unhandled Variable while serializing!");
            }
        }
    }

    template <typename... Components>
    FORCEINLINE void SerializeComponents(nlohmann::json& out, Entity& e)
    {
        (SerializeComponent<Components>(out, e), ...);
    }

    // nlohmann::json& j is in "Scene" scope
    [[maybe_unused]] static void SerializeEntity(nlohmann::json& j, Entity& e, uint64_t index)
    {
        SG_ASSERT_NOMSG(e);

        nlohmann::json& out = j[std::format("Entity{0}", index)];
        SerializeComponents<SERIALIZABLE_COMPONENTS>(out, e);
    }

    void Serializer::SerializeScene([[maybe_unused]] const Path& path, [[maybe_unused]] Scene* in)
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

        // Size of all entities in the scene
        const size_t size = in->GetRegistry().view<IDComponent>().size();
        outJson["Scene"]["Size"] = size;
        String result = outJson.dump(4);

        if(!Filesystem::WriteTextFile(path, result))
            Log<Severity::Error>("SerializeScene: Failed to Write Text File {}", path.string());
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

        if constexpr(std::is_same_v<XComponent, RelationshipComponent>)
        {
            // (Rid) IMPORTANT: Do absolutely nothing here! 
            // We MUST wait for all entities to exist before linking them
            return;
        }

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
            else if(type.EqualTo<glm::vec2>())
                *reinterpret_cast<glm::vec2*>(dest) = inJson.value(name, glm::vec2(0.0f));
            else if(type.EqualTo<glm::vec3>())
                *reinterpret_cast<glm::vec3*>(dest) = inJson.value(name, glm::vec3(0.0f));
            else if(type.EqualTo<glm::vec4>())
                *reinterpret_cast<glm::vec4*>(dest) = inJson.value(name, glm::vec4(0.0f));
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
            else if(type.EqualTo<RigidbodyType>())
                *reinterpret_cast<RigidbodyType*>(dest) = inJson.value(name, RigidbodyType::STATIC);
            else if(type.EqualTo<TextAlignment>())
                *reinterpret_cast<TextAlignment*>(dest) = inJson.value(name, TextAlignment::LEFT);
            else if(type.EqualTo<TextVerticalAlignment>())
                *reinterpret_cast<TextVerticalAlignment*>(dest) = inJson.value(name, TextVerticalAlignment::BASELINE);
            else if(type.EqualTo<Ref<Asset>>())
            {
                AssetManager* am = Core::GetAssetManager();
                AssetID assetID = inJson.value(name, 0ULL);
                const AssetMetadata& metadata = am->GetMetadata(assetID);

                if(assetID == AssetID(AssetID::INVALID))
                {
                    *reinterpret_cast<Ref<Asset>*>(dest) = nullptr;
                    continue;
                }

                if(metadata.IsValid() && !metadata.IsMissing())
                {
                    if (metadata.Type == AssetType::TEXTURE2D)
                        *reinterpret_cast<Ref<Asset>*>(dest) = am->Load<Texture2D>(assetID);
                    if (metadata.Type == AssetType::MESH)
                        *reinterpret_cast<Ref<Asset>*>(dest) = am->Load<Mesh>(assetID);
                    if (metadata.Type == AssetType::FONT)
                        *reinterpret_cast<Ref<Asset>*>(dest) = am->Load<Font>(assetID);
                    if (metadata.Type == AssetType::SCRIPT)
                        *reinterpret_cast<Ref<Asset>*>(dest) = am->Load<Script>(assetID);
                }

                else
                {
                    Log<Severity::Warn>("DeserializeComponent: Asset with ID {} is missing or invalid", assetID.Get());
                    *reinterpret_cast<Ref<Asset>*>(dest) = nullptr;
                }
            }
            else
                Log<Severity::Warn>("DeserializeComponent: Unhandled type {} for field {}", type.GetFullName(), name);
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
        if(!Filesystem::ReadTextFile(path, jsonContents))
        {
            Log<Severity::Error>("DeserializeScene: Failed to read file '{}'", path.string());
            return;
        }

        nlohmann::json parsedJson = nlohmann::json::parse(jsonContents, nullptr, false);
        if(parsedJson.is_discarded())
        {
            Log<Severity::Error>("DeserializeScene: Corrupt or invalid JSON at '{}'", path.string());
            return;
        }

        // (Rid) 1st pass: Create flat hierarchy of entities, skip RelationshipComponent
        uint64_t sceneSize = parsedJson["Scene"]["Size"];
        for(uint64_t i = 0; i < sceneSize; i++)
        {
            Entity newEntity;
            out->CreateEntity(newEntity, "");
            DeserializeEntity(parsedJson["Scene"], newEntity, i);
        }

        // (Rid) 2nd pass: parse the RelationshipComponent in a second pass, once all entities exist
        for(uint64_t i = 0; i < sceneSize; i++)
        {
            nlohmann::json& entityJson = parsedJson["Scene"][std::format("Entity{0}", i)];
            if(entityJson.contains("Surge::RelationshipComponent"))
            {
                uint64_t uuid = entityJson["Surge::IDComponent"]["ID"].get<uint64_t>();
                Entity entity = out->FindEntityByUUID(uuid);
                SG_ASSERT_NOMSG(entity);
                if(entity)
                {
                    auto& relJson = entityJson["Surge::RelationshipComponent"];
                    auto& rel = entity.GetComponent<RelationshipComponent>();
                    auto GetHandle = [&](uint64_t savedUUID) -> entt::entity {
                        if(savedUUID == 0)
                            return entt::null;
                        Entity found = out->FindEntityByUUID(savedUUID);
                        return found ? found.Raw() : entt::null;
                    };

                    rel.Parent = (Uint)GetHandle(relJson.value("Parent", UUID::INVALID));
                    rel.FirstChild = (Uint)GetHandle(relJson.value("FirstChild", UUID::INVALID));
                    rel.PreviousSibling = (Uint)GetHandle(relJson.value("PreviousSibling", UUID::INVALID));
                    rel.NextSibling = (Uint)GetHandle(relJson.value("NextSibling", UUID::INVALID));
                    rel.ChildrenCount = relJson.value("ChildrenCount", 0);
                }
            }
        }
    }
#pragma endregion

#pragma region ProjectSerializer
    void Serializer::SerializeProject([[maybe_unused]] const Path& path, [[maybe_unused]] Project* in)
    {
#ifdef SURGE_PLATFORM_ANDROID
        Log<Severity::Error>("[Serializer] SerializeProject is unsupported on Android runtime. APK assets are readonly!");
#else
        SG_ASSERT_NOMSG(in);

        nlohmann::json outJson;
        outJson["Project"]["Name"] = in->Name;
        outJson["Project"]["Version"] = in->Version;
        outJson["Project"]["StartScene"] = static_cast<uint64_t>(in->StartScene);

        String result = outJson.dump(4);

        if (!Filesystem::WriteTextFile(path, result))
            Log<Severity::Error>("SerializeProject: Failed to write to '{}'", path.string());
#endif
    }

    void Serializer::DeserializeProject(const Path& path, Project* out)
    {
        SG_ASSERT_NOMSG(out);

        String jsonContents;
        if (!Filesystem::ReadTextFile(path, jsonContents))
        {
            Log<Severity::Error>("DeserializeProject: Failed to read file '{}'", path.string());
            return;
        }

        nlohmann::json parsedJson = nlohmann::json::parse(jsonContents);
        if(parsedJson.is_discarded())
        {
            Log<Severity::Error>("DeserializeProject: Corrupt or invalid JSON at '{}'", path.string());
            return;
        }

        if(parsedJson.contains("Project"))
        {
            const auto& projNode = parsedJson["Project"];
            out->Name = projNode.value("Name", "");
            out->Version = projNode.value("Version", "1.0");
            out->StartScene = static_cast<AssetID>(projNode.value("StartScene", 0ULL));
        }
        else
            Log<Severity::Error>("DeserializeProject: Corrupted file '{}'", path.string());
    }

#pragma endregion

} // namespace Surge