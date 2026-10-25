// Copyright (c) - SurgeTechnologies - All rights reserved
#include "SceneSerializer.hpp"
#include "SceneBinaryFormat.hpp"
#include "SerializableComponents.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/ECS/Scene.hpp"
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
    //////////////////////////////////////////////////////////////////////////
    // JSON source (.srg): the Editor's authoring format, also the entity snapshots of the Editor's undo/copy-paste
    //////////////////////////////////////////////////////////////////////////

    // Enum Mappings
    NLOHMANN_JSON_SERIALIZE_ENUM(RuntimeCamera::ProjectionType, { {RuntimeCamera::ProjectionType::Perspective, "Perspective"}, {RuntimeCamera::ProjectionType::Orthographic, "Orthographic"} });
    NLOHMANN_JSON_SERIALIZE_ENUM(LightType, { {LightType::POINT, "POINT"}, {LightType::DIRECTIONAL, "DIRECTIONAL"} });
    NLOHMANN_JSON_SERIALIZE_ENUM(RigidbodyType, { {RigidbodyType::STATIC, "STATIC"}, {RigidbodyType::DYNAMIC, "DYNAMIC"}, {RigidbodyType::KINEMATIC, "KINEMATIC"} });
    NLOHMANN_JSON_SERIALIZE_ENUM(TextAlignment, { {TextAlignment::LEFT, "LEFT"}, {TextAlignment::CENTER, "CENTER"}, {TextAlignment::RIGHT, "RIGHT"} });
    NLOHMANN_JSON_SERIALIZE_ENUM(TextVerticalAlignment, { {TextVerticalAlignment::TOP, "TOP"}, {TextVerticalAlignment::CENTER, "CENTER"}, {TextVerticalAlignment::BOTTOM, "BOTTOM"}, {TextVerticalAlignment::BASELINE, "BASELINE"} });
    NLOHMANN_JSON_SERIALIZE_ENUM(ParticleCullingMode, { {ParticleCullingMode::ALWAYS_SIMULATE, "ALWAYS_SIMULATE"}, {ParticleCullingMode::PAUSE_OFFSCREEN, "PAUSE_OFFSCREEN"}, {ParticleCullingMode::PAUSE_AND_CATCH_UP, "PAUSE_AND_CATCH_UP"} });
    NLOHMANN_JSON_SERIALIZE_ENUM(ShadowQuality, { {ShadowQuality::LOW, "LOW"}, {ShadowQuality::MEDIUM, "MEDIUM"}, {ShadowQuality::HIGH, "HIGH"}, {ShadowQuality::ULTRA, "ULTRA"} });
    NLOHMANN_JSON_SERIALIZE_ENUM(AOQuality, { {AOQuality::LOW, "LOW"}, {AOQuality::MEDIUM, "MEDIUM"}, {AOQuality::HIGH, "HIGH"}, {AOQuality::ULTRA, "ULTRA"} });
    NLOHMANN_JSON_SERIALIZE_ENUM(BloomQuality, { {BloomQuality::LOW, "LOW"}, {BloomQuality::MEDIUM, "MEDIUM"}, {BloomQuality::HIGH, "HIGH"}, {BloomQuality::ULTRA, "ULTRA"} });
    NLOHMANN_JSON_SERIALIZE_ENUM(AttenuationModel, { {AttenuationModel::NONE, "NONE"}, {AttenuationModel::INVERSE_DISTANCE, "INVERSE_DISTANCE"}, {AttenuationModel::LINEAR_DISTANCE, "LINEAR_DISTANCE"}, {AttenuationModel::EXPONENTIAL_DISTANCE, "EXPONENTIAL_DISTANCE"} });

    // Inspector values of a script's Properties, only written when there are any (older scenes stay untouched)
    // Nested by group ({ "Shadows": { "Enable": true, ... } }). A missing key keeps the field's default, so settings added later load into older scenes
    static nlohmann::json RenderSettingsToJson(const RenderSettings& settings)
    {
        nlohmann::json out = nlohmann::json::object();
        RenderSettings::VisitSavedFields(settings, [&out](const char* group, const char* name, const auto& value) { out[group][name] = value; });
        return out;
    }

    static void RenderSettingsFromJson(const nlohmann::json& in, RenderSettings& settings)
    {
        RenderSettings::VisitSavedFields(settings, [&in](const char* group, const char* name, auto& value) {
            const auto groupItr = in.find(group);
            if(groupItr == in.end())
                return;
            const auto valueItr = groupItr->find(name);
            if(valueItr != groupItr->end())
                value = valueItr->template get<std::remove_reference_t<decltype(value)>>();
        });
    }

    static nlohmann::json FieldValueToJson(ScriptFieldType type, ScriptFieldType elementType, const ScriptFieldValue& value)
    {
        switch(type)
        {
            case ScriptFieldType::FLOAT:
            case ScriptFieldType::INT: return value.Number;
            case ScriptFieldType::BOOL: return value.Bool;
            case ScriptFieldType::STRING:
            case ScriptFieldType::ENUM: return value.Text;
            case ScriptFieldType::VEC2:
            case ScriptFieldType::VEC3:
            case ScriptFieldType::VEC4:
            case ScriptFieldType::COLOR: return value.Vector;
            case ScriptFieldType::ENTITY: return value.EntityID;
            case ScriptFieldType::ASSET: return value.AssetID;
            case ScriptFieldType::ARRAY:
            {
                nlohmann::json array = nlohmann::json::array();
                for(const ScriptFieldValue& element : value.Elements)
                    array.push_back(FieldValueToJson(elementType, ScriptFieldType::FLOAT, element));
                return array;
            }
            default: return nullptr;
        }
    }

    static bool FieldValueFromJson(const nlohmann::json& json, ScriptFieldType type, ScriptFieldType elementType, ScriptFieldValue& out)
    {
        switch(type)
        {
            case ScriptFieldType::FLOAT:
            case ScriptFieldType::INT: if(!json.is_number()) return false; out.Number = json.get<double>(); return true;
            case ScriptFieldType::BOOL: if(!json.is_boolean()) return false; out.Bool = json.get<bool>(); return true;
            case ScriptFieldType::STRING:
            case ScriptFieldType::ENUM: if(!json.is_string()) return false; out.Text = json.get<String>(); return true;
            case ScriptFieldType::VEC2:
            case ScriptFieldType::VEC3:
            case ScriptFieldType::VEC4:
            case ScriptFieldType::COLOR: if(!json.is_object()) return false; out.Vector = json.get<glm::vec4>(); return true;
            case ScriptFieldType::ENTITY: if(!json.is_number()) return false; out.EntityID = json.get<uint64_t>(); return true;
            case ScriptFieldType::ASSET: if(!json.is_number()) return false; out.AssetID = json.get<uint64_t>(); return true;
            case ScriptFieldType::ARRAY:
            {
                if(!json.is_array())
                    return false;
                for(const nlohmann::json& element : json)
                {
                    ScriptFieldValue item;
                    if(FieldValueFromJson(element, elementType, ScriptFieldType::FLOAT, item))
                        out.Elements.push_back(std::move(item));
                }
                return true;
            }
            default: return false;
        }
    }

    // Inspector values of a script's Properties, only written when there are any (older scenes stay untouched)
    static void SerializeScriptFields(nlohmann::json& out, const Vector<ScriptFieldOverride>& fields)
    {
        if(fields.empty())
            return;

        nlohmann::json& array = out["Fields"];
        for(const ScriptFieldOverride& field : fields)
        {
            nlohmann::json entry;
            entry["Name"] = field.Name;
            entry["Type"] = ScriptFieldTypeToString(field.Type);
            if(field.Type == ScriptFieldType::ARRAY)
                entry["ElementType"] = ScriptFieldTypeToString(field.ElementType);
            entry["Value"] = FieldValueToJson(field.Type, field.ElementType, field.Value);
            array.push_back(std::move(entry));
        }
    }

    static void DeserializeScriptFields(const nlohmann::json& in, Vector<ScriptFieldOverride>& outFields)
    {
        outFields.clear();
        if(!in.contains("Fields") || !in["Fields"].is_array())
            return;

        for(const nlohmann::json& entry : in["Fields"])
        {
            ScriptFieldOverride field;
            field.Name = entry.value("Name", String());
            bool valid = !field.Name.empty() && entry.contains("Value") && ScriptFieldTypeFromString(entry.value("Type", String()).c_str(), field.Type);
            if(valid && field.Type == ScriptFieldType::ARRAY)
                valid = ScriptFieldTypeFromString(entry.value("ElementType", String()).c_str(), field.ElementType);
            if(!valid || !FieldValueFromJson(entry["Value"], field.Type, field.ElementType, field.Value))
            {
                Log<Severity::Warn>("[Serializer] Skipping a malformed script field '{}'", field.Name);
                continue;
            }
            outFields.push_back(std::move(field));
        }
    }

    static Ref<Asset> LoadScriptAsset(AssetManager& assetManager, uint64_t rawID)
    {
        return LoadSerializedAsset(assetManager, AssetID(rawID), AssetType::SCRIPT);
    }

    // "Scripts": [ { "Active", "ScriptAsset", "Fields" }, ... ]
    static void SerializeScriptList(nlohmann::json& out, const ScriptComponent& component)
    {
        nlohmann::json& scripts = out["Scripts"];
        scripts = nlohmann::json::array();
        for(const ScriptEntry& entry : component.Scripts)
        {
            nlohmann::json item;
            item["Active"] = entry.Active;
            item["ScriptAsset"] = entry.ScriptAsset ? entry.ScriptAsset->GetID().Get() : static_cast<uint64_t>(AssetID::INVALID);
            SerializeScriptFields(item, entry.Fields);
            scripts.push_back(std::move(item));
        }
    }

    static void DeserializeScriptList(AssetManager& assetManager, const nlohmann::json& in, ScriptComponent& component)
    {
        component.Scripts.clear();
        if(in.contains("Scripts") && in["Scripts"].is_array())
        {
            for(const nlohmann::json& item : in["Scripts"])
            {
                ScriptEntry& entry = component.Scripts.emplace_back();
                entry.Active = item.value("Active", true);
                entry.ScriptAsset = LoadScriptAsset(assetManager, item.value("ScriptAsset", 0ULL));
                DeserializeScriptFields(item, entry.Fields);
            }
        }
        else if(in.contains("ScriptAsset")) // Scenes from before multiple scripts per entity: one script, its fields next to it
        {
            ScriptEntry& entry = component.Scripts.emplace_back();
            entry.ScriptAsset = LoadScriptAsset(assetManager, in.value("ScriptAsset", 0ULL));
            DeserializeScriptFields(in, entry.Fields);
        }
    }

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
                else if (type.EqualTo<int>())
                    out[name] = *reinterpret_cast<const int*>(source);
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
                else if(type.EqualTo<AttenuationModel>())
                    out[name] = *reinterpret_cast<const AttenuationModel*>(source);
                else if(type.EqualTo<RigidbodyType>())
                    out[name] = *reinterpret_cast<const RigidbodyType*>(source);
                else if(type.EqualTo<TextAlignment>())
                    out[name] = *reinterpret_cast<const TextAlignment*>(source);
                else if(type.EqualTo<TextVerticalAlignment>())
                    out[name] = *reinterpret_cast<const TextVerticalAlignment*>(source);
                else if(type.EqualTo<ParticleCullingMode>())
                    out[name] = *reinterpret_cast<const ParticleCullingMode*>(source);
                else if(type.EqualTo<RenderSettings>())
                    out[name] = RenderSettingsToJson(*reinterpret_cast<const RenderSettings*>(source));
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

            if constexpr(std::is_same_v<XComponent, ScriptComponent>)
                SerializeScriptList(out, comp);
            else if constexpr(std::is_same_v<XComponent, UICanvasComponent>)
                SerializeScriptFields(out, comp.Fields);
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

    String SceneSerializer::ToJson(Scene& scene)
    {
        SCOPED_TIMER("SceneSerializer::ToJson");
        nlohmann::json outJson = nlohmann::json();

        uint64_t index = 0;
        scene.GetRegistry().each([&](auto entityID) {
            Entity e = Entity(entityID, &scene);
            SerializeEntity(outJson["Scene"], e, index);
            index++;
        });

        // Size of all entities in the scene
        const size_t size = scene.GetRegistry().view<IDComponent>().size();
        outJson["Scene"]["Size"] = size;
        return outJson.dump(4);
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
        AssetManager& assetManager = *e.GetScene()->GetContext().AssetManager_;

        if constexpr(std::is_same_v<XComponent, RelationshipComponent>)
        {
            // (Rid) IMPORTANT: Do absolutely nothing here! 
            // We MUST wait for all entities to exist before linking them
            return;
        }

        if constexpr(std::is_same_v<XComponent, ScriptComponent>)
            DeserializeScriptList(assetManager, inJson, comp);
        else if constexpr(std::is_same_v<XComponent, UICanvasComponent>)
            DeserializeScriptFields(inJson, comp.Fields);

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
            else if(type.EqualTo<int>())
                *reinterpret_cast<int*>(dest) = inJson.value(name, 0);
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
            else if(type.EqualTo<AttenuationModel>())
                *reinterpret_cast<AttenuationModel*>(dest) = inJson.value(name, AttenuationModel::INVERSE_DISTANCE);
            else if(type.EqualTo<RigidbodyType>())
                *reinterpret_cast<RigidbodyType*>(dest) = inJson.value(name, RigidbodyType::STATIC);
            else if(type.EqualTo<TextAlignment>())
                *reinterpret_cast<TextAlignment*>(dest) = inJson.value(name, TextAlignment::LEFT);
            else if(type.EqualTo<TextVerticalAlignment>())
                *reinterpret_cast<TextVerticalAlignment*>(dest) = inJson.value(name, TextVerticalAlignment::BASELINE);
            else if(type.EqualTo<ParticleCullingMode>())
                *reinterpret_cast<ParticleCullingMode*>(dest) = inJson.value(name, ParticleCullingMode::PAUSE_OFFSCREEN);
            else if(type.EqualTo<RenderSettings>())
                RenderSettingsFromJson(inJson[name], *reinterpret_cast<RenderSettings*>(dest));
            else if(type.EqualTo<Ref<Asset>>())
                *reinterpret_cast<Ref<Asset>*>(dest) = LoadSerializedAsset(assetManager, AssetID(inJson.value(name, 0ULL)));
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

    bool SceneSerializer::FromJson(const String& json, Scene& outScene)
    {
        Scene* out = &outScene;
        auto& registry = out->GetRegistry();
        registry.clear();

        nlohmann::json parsedJson = nlohmann::json::parse(json, nullptr, false);
        if(parsedJson.is_discarded())
        {
            Log<Severity::Error>("[SceneSerializer] FromJson: Corrupt or invalid scene JSON");
            return false;
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
        return true;
    }

    template <typename XComponent>
    static void RemoveComponentIfMissing(const nlohmann::json& j, Entity& e)
    {
        if constexpr(std::is_same_v<XComponent, IDComponent> || std::is_same_v<XComponent, RelationshipComponent>)
            return;
        else if(e.HasComponent<XComponent>() && !j.contains(SurgeReflect::GetReflection<XComponent>()->GetName()))
            e.RemoveComponent<XComponent>();
    }

    template <typename... Components>
    FORCEINLINE void RemoveComponentsIfMissing(const nlohmann::json& j, Entity& e)
    {
        (RemoveComponentIfMissing<Components>(j, e), ...);
    }

    String SceneSerializer::EntityToJson(Entity entity)
    {
        SG_ASSERT_NOMSG(entity);
        nlohmann::json out = nlohmann::json::object();
        SerializeComponents<SERIALIZABLE_COMPONENTS>(out, entity);
        out.erase(SurgeReflect::GetReflection<RelationshipComponent>()->GetName()); // Links are entt handles, meaningless outside this registry
        return out.dump();
    }

    bool SceneSerializer::EntityFromJson(const String& data, Entity entity)
    {
        SG_ASSERT_NOMSG(entity);
        nlohmann::json in = nlohmann::json::parse(data, nullptr, false);
        if(in.is_discarded())
        {
            Log<Severity::Error>("[SceneSerializer] EntityFromJson: Corrupt or invalid entity data");
            return false;
        }

        const UUID id = entity.GetComponent<IDComponent>().ID;
        RemoveComponentsIfMissing<SERIALIZABLE_COMPONENTS>(in, entity);
        DeserializeComponents<SERIALIZABLE_COMPONENTS>(in, entity);
        entity.GetComponent<IDComponent>().ID = id;

        if(entity.HasComponent<TransformComponent>())
            entity.GetComponent<TransformComponent>().MarkDirty();
        if(entity.HasComponent<ConvexColliderComponent>())
            entity.GetComponent<ConvexColliderComponent>().IsDirty = true;

        return true;
    }

    //////////////////////////////////////////////////////////////////////////
    // SceneSerializer
    //////////////////////////////////////////////////////////////////////////

    void SceneSerializer::Initialize()
    {
        mSerializerType = AssetType::SCENE;
    }

    bool SceneSerializer::Serialize([[maybe_unused]] Ref<Asset> asset) const
    {
#ifdef SURGE_PLATFORM_ANDROID
        Log<Severity::Error>("[SceneSerializer] Serialize is unsupported on Android runtime. APK assets are readonly!");
        return false;
#else
        AssetManager* assetManager = mContext->AssetManager_;
        const AssetMetadata& metadata = assetManager->GetMetadata(asset->GetID());
        const String absolutePath = assetManager->GetAbsolutePath(metadata.RelativePath);
        Ref<Scene> scene = asset.As<Scene>();

        // JSON source first: the cooked file is stamped with its new modification time, so the next Load does not re-cook it
        if(!Filesystem::WriteTextFile(absolutePath, ToJson(*scene)))
        {
            Log<Severity::Error>("[SceneSerializer] Failed to write {}", absolutePath);
            return false;
        }

        const AssetStamp stamp = AssetStampWriter::Build(absolutePath, SceneBinary::GetFormatVersion());
        return SceneBinary::Write(assetManager->GetSidecarPath(metadata.ID), stamp, scene.Raw());
#endif
    }

    Ref<Asset> SceneSerializer::Deserialize(const AssetMetadata& metadata) const
    {
        AssetManager* assetManager = mContext->AssetManager_;

        Ref<Scene> scene = Ref<Scene>::Create(Filesystem::GetFilenameWithoutExt(metadata.RelativePath), mContext->Scenes);
        AssetStamp stamp;
        if(!SceneBinary::Read(assetManager->GetSidecarPath(metadata.ID), stamp, scene.Raw()))
            return nullptr;
        return scene.As<Asset>();
    }

    void SceneSerializer::Shutdown()
    {
        Log<Severity::Info>("[SceneSerializer] Shutdown");
    }

} // namespace Surge
