// Copyright (c) - SurgeTechnologies - All rights reserved
#include "SceneFormat.hpp"
#include "Surge/Asset/AssetContext.hpp"
#include "Surge/Asset/Format/BinarySerialization.hpp"
#include "Surge/Asset/Format/Scene/SerializableComponents.hpp"
#include "Surge/ECS/Scene.hpp"
#include <unordered_map>
#include <cstring>

namespace Surge::SceneBinary
{

    template <typename... Components>
    struct ComponentCount
    {
        static constexpr size_t VALUE = sizeof...(Components);
    };
    static_assert(ComponentCount<SERIALIZABLE_COMPONENTS>::VALUE <= 32, "SceneBinary stores one bit per serializable component in a Uint mask");

    // Relationship links are entt handles, only resolvable once every entity of the file exists
    struct PendingLinks
    {
        entt::entity Entity = entt::null;
        uint64_t Parent = 0;
        uint64_t FirstChild = 0;
        uint64_t PreviousSibling = 0;
        uint64_t NextSibling = 0;
        Uint ChildrenCount = 0;
    };

    //////////////////////////////////////////////////////////////////////////
    // Schema
    //////////////////////////////////////////////////////////////////////////

    static void HashBytes(Uint& hash, const void* data, size_t size) // FNV-1a
    {
        const Byte* bytes = static_cast<const Byte*>(data);
        for(size_t i = 0; i < size; i++)
        {
            hash ^= bytes[i];
            hash *= 16777619u;
        }
    }

    // Field names + sizes of RenderSettings::VisitSavedFields: changing that list re-cooks every scene, like changing a component
    static void HashRenderSettingsLayout(Uint& hash)
    {
        const RenderSettings settings;
        RenderSettings::VisitSavedFields(settings, [&hash](const char* group, const char* name, const auto& value) {
            const Uint size = static_cast<Uint>(sizeof(value));
            HashBytes(hash, group, std::strlen(group));
            HashBytes(hash, name, std::strlen(name));
            HashBytes(hash, &size, sizeof(size));
        });
    }

    template <typename T>
    static void HashComponent(Uint& hash)
    {
        const String& name = SurgeReflect::GetReflection<T>()->GetName();
        HashBytes(hash, name.data(), name.size());
        for(const SerializedVariable& var : GetSerializedVariables<T>())
        {
            HashBytes(hash, var.Name->data(), var.Name->size());
            HashBytes(hash, &var.Kind, sizeof(var.Kind));
            if(var.Kind == SerializedFieldKind::RENDER_SETTINGS)
                HashRenderSettingsLayout(hash);
        }
    }

    template <typename... Components>
    static Uint ComputeSchemaHash()
    {
        Uint hash = 2166136261u;
        (HashComponent<Components>(hash), ...);
        return hash;
    }

    static Uint GetSchemaHash()
    {
        static const Uint sHash = ComputeSchemaHash<SERIALIZABLE_COMPONENTS>();
        return sHash;
    }

    Uint GetSchemaVersion()
    {
        Uint hash = GetSchemaHash();
        HashBytes(hash, &VERSION, sizeof(VERSION));
        return hash;
    }

    //////////////////////////////////////////////////////////////////////////
    // Fields
    //////////////////////////////////////////////////////////////////////////

    template <typename T>
    static void WriteAs(Rapt0R::Writer& writer, const Byte* src)
    {
        writer.Write(*reinterpret_cast<const T*>(src));
    }

    template <typename T>
    static void ReadAs(Rapt0R::Reader& reader, Byte* dest)
    {
        reader.Read(*reinterpret_cast<T*>(dest));
    }

    static void WriteField(Rapt0R::Writer& writer, SerializedFieldKind kind, const Byte* src)
    {
        switch(kind)
        {
            case SerializedFieldKind::BOOL: WriteAs<bool>(writer, src); break;
            case SerializedFieldKind::FLOAT: WriteAs<float>(writer, src); break;
            case SerializedFieldKind::INT: WriteAs<int>(writer, src); break;
            case SerializedFieldKind::UUID: WriteAs<UUID>(writer, src); break;
            case SerializedFieldKind::STRING: WriteAs<String>(writer, src); break;
            case SerializedFieldKind::VEC2: WriteAs<glm::vec2>(writer, src); break;
            case SerializedFieldKind::VEC3: WriteAs<glm::vec3>(writer, src); break;
            case SerializedFieldKind::VEC4: WriteAs<glm::vec4>(writer, src); break;
            case SerializedFieldKind::RUNTIME_CAMERA:
            {
                const RuntimeCamera* cam = reinterpret_cast<const RuntimeCamera*>(src);
                writer.Write(cam->GetPerspectiveVerticalFOV(), cam->GetPerspectiveNearClip(), cam->GetPerspectiveFarClip(), cam->GetOrthographicNearClip(),
                             cam->GetOrthographicFarClip(), cam->GetOrthographicSize(), cam->GetProjectionType());
                break;
            }
            case SerializedFieldKind::LIGHT_TYPE: WriteAs<LightType>(writer, src); break;
            case SerializedFieldKind::ATTENUATION_MODEL: WriteAs<AttenuationModel>(writer, src); break;
            case SerializedFieldKind::RIGIDBODY_TYPE: WriteAs<RigidbodyType>(writer, src); break;
            case SerializedFieldKind::TEXT_ALIGNMENT: WriteAs<TextAlignment>(writer, src); break;
            case SerializedFieldKind::TEXT_VERTICAL_ALIGNMENT: WriteAs<TextVerticalAlignment>(writer, src); break;
            case SerializedFieldKind::PARTICLE_CULLING_MODE: WriteAs<ParticleCullingMode>(writer, src); break;
            case SerializedFieldKind::ASSET_REF:
            {
                const Ref<Asset>& asset = *reinterpret_cast<const Ref<Asset>*>(src);
                writer.Write(asset ? asset->GetID().Get() : static_cast<uint64_t>(AssetID::INVALID));
                break;
            }
            case SerializedFieldKind::RENDER_SETTINGS:
                RenderSettings::VisitSavedFields(*reinterpret_cast<const RenderSettings*>(src), [&writer](const char*, const char*, const auto& value) { writer.Write(value); });
                break;
            case SerializedFieldKind::UNKNOWN: break; // Not saved, the JSON format skips it too
        }
    }

    static void ReadField(Rapt0R::Reader& reader, AssetManager& assetManager, SerializedFieldKind kind, Byte* dest)
    {
        switch(kind)
        {
            case SerializedFieldKind::BOOL: ReadAs<bool>(reader, dest); break;
            case SerializedFieldKind::FLOAT: ReadAs<float>(reader, dest); break;
            case SerializedFieldKind::INT: ReadAs<int>(reader, dest); break;
            case SerializedFieldKind::UUID: ReadAs<UUID>(reader, dest); break;
            case SerializedFieldKind::STRING: ReadAs<String>(reader, dest); break;
            case SerializedFieldKind::VEC2: ReadAs<glm::vec2>(reader, dest); break;
            case SerializedFieldKind::VEC3: ReadAs<glm::vec3>(reader, dest); break;
            case SerializedFieldKind::VEC4: ReadAs<glm::vec4>(reader, dest); break;
            case SerializedFieldKind::RUNTIME_CAMERA:
            {
                float fov = 0.0f, nearClip = 0.0f, farClip = 0.0f, orthoNear = 0.0f, orthoFar = 0.0f, orthoSize = 0.0f;
                RuntimeCamera::ProjectionType projection = RuntimeCamera::ProjectionType::Perspective;
                if(!reader.Read(fov, nearClip, farClip, orthoNear, orthoFar, orthoSize, projection))
                    break;

                RuntimeCamera* cam = reinterpret_cast<RuntimeCamera*>(dest);
                cam->SetPerspectiveVerticalFOV(fov);
                cam->SetPerspectiveNearClip(nearClip);
                cam->SetPerspectiveFarClip(farClip);
                cam->SetOrthographicNearClip(orthoNear);
                cam->SetOrthographicFarClip(orthoFar);
                cam->SetOrthographicSize(orthoSize);
                cam->SetProjectionType(projection);
                break;
            }
            case SerializedFieldKind::LIGHT_TYPE: ReadAs<LightType>(reader, dest); break;
            case SerializedFieldKind::ATTENUATION_MODEL: ReadAs<AttenuationModel>(reader, dest); break;
            case SerializedFieldKind::RIGIDBODY_TYPE: ReadAs<RigidbodyType>(reader, dest); break;
            case SerializedFieldKind::TEXT_ALIGNMENT: ReadAs<TextAlignment>(reader, dest); break;
            case SerializedFieldKind::TEXT_VERTICAL_ALIGNMENT: ReadAs<TextVerticalAlignment>(reader, dest); break;
            case SerializedFieldKind::PARTICLE_CULLING_MODE: ReadAs<ParticleCullingMode>(reader, dest); break;
            case SerializedFieldKind::ASSET_REF:
            {
                uint64_t rawID = 0;
                const bool read = reader.Read(rawID);
                *reinterpret_cast<Ref<Asset>*>(dest) = read ? LoadSerializedAsset(assetManager, AssetID(rawID)) : nullptr;
                break;
            }
            case SerializedFieldKind::RENDER_SETTINGS:
                RenderSettings::VisitSavedFields(*reinterpret_cast<RenderSettings*>(dest), [&reader](const char*, const char*, auto& value) { reader.Read(value); });
                break;
            case SerializedFieldKind::UNKNOWN: break;
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // Script fields
    //////////////////////////////////////////////////////////////////////////

    static void WriteFieldValue(Rapt0R::Writer& writer, ScriptFieldType type, ScriptFieldType elementType, const ScriptFieldValue& value)
    {
        switch(type)
        {
            case ScriptFieldType::FLOAT:
            case ScriptFieldType::INT: writer.Write(value.Number); break;
            case ScriptFieldType::BOOL: writer.Write(value.Bool); break;
            case ScriptFieldType::STRING:
            case ScriptFieldType::ENUM: writer.Write(value.Text); break;
            case ScriptFieldType::VEC2:
            case ScriptFieldType::VEC3:
            case ScriptFieldType::VEC4:
            case ScriptFieldType::COLOR: writer.Write(value.Vector); break;
            case ScriptFieldType::ENTITY: writer.Write(value.EntityID); break;
            case ScriptFieldType::ASSET: writer.Write(value.AssetID); break;
            case ScriptFieldType::ARRAY:
            {
                writer.Write(static_cast<Uint>(value.Elements.size()));
                for(const ScriptFieldValue& element : value.Elements)
                    WriteFieldValue(writer, elementType, ScriptFieldType::FLOAT, element);
                break;
            }
            default: break;
        }
    }

    static void ReadFieldValue(Rapt0R::Reader& reader, ScriptFieldType type, ScriptFieldType elementType, ScriptFieldValue& out)
    {
        switch(type)
        {
            case ScriptFieldType::FLOAT:
            case ScriptFieldType::INT: reader.Read(out.Number); break;
            case ScriptFieldType::BOOL: reader.Read(out.Bool); break;
            case ScriptFieldType::STRING:
            case ScriptFieldType::ENUM: reader.Read(out.Text); break;
            case ScriptFieldType::VEC2:
            case ScriptFieldType::VEC3:
            case ScriptFieldType::VEC4:
            case ScriptFieldType::COLOR: reader.Read(out.Vector); break;
            case ScriptFieldType::ENTITY: reader.Read(out.EntityID); break;
            case ScriptFieldType::ASSET: reader.Read(out.AssetID); break;
            case ScriptFieldType::ARRAY:
            {
                Uint count = 0;
                if(!reader.Read(count) || !reader.EnsureElements(count))
                    break;

                out.Elements.resize(count);
                for(Uint i = 0; i < count && !reader.HasError(); i++)
                    ReadFieldValue(reader, elementType, ScriptFieldType::FLOAT, out.Elements[i]);
                break;
            }
            default: break;
        }
    }
}

namespace Surge
{
    // Found by Rapt0R through ADL: the script lists below write/read their Vector<ScriptFieldOverride> with a plain Write/Read
    static void Rapt0RSerialize(Rapt0R::Writer& writer, const ScriptFieldOverride& field)
    {
        writer.Write(field.Name, field.Type, field.ElementType);
        SceneBinary::WriteFieldValue(writer, field.Type, field.ElementType, field.Value);
    }

    static void Rapt0RDeserialize(Rapt0R::Reader& reader, ScriptFieldOverride& field)
    {
        if(!reader.Read(field.Name, field.Type, field.ElementType))
            return;

        if(field.Type >= ScriptFieldType::COUNT || field.ElementType >= ScriptFieldType::COUNT)
        {
            reader.SetError(Rapt0R::ErrorCode::CORRUPT_DATA);
            return;
        }
        SceneBinary::ReadFieldValue(reader, field.Type, field.ElementType, field.Value);
    }
}

namespace Surge::SceneBinary
{
    //////////////////////////////////////////////////////////////////////////
    // Components
    //////////////////////////////////////////////////////////////////////////

    template <typename T>
    static void WriteComponent(Rapt0R::Writer& writer, Entity& entity)
    {
        if(!entity.HasComponent<T>())
            return;

        const T& comp = entity.GetComponent<T>();
        if constexpr(std::is_same_v<T, RelationshipComponent>)
        {
            Scene* scene = entity.GetScene();
            auto GetUUID = [scene](Uint handle) -> uint64_t
            {
                const entt::entity e = static_cast<entt::entity>(handle);
                if(e == entt::null || !scene->GetRegistry().valid(e))
                    return UUID::INVALID;
                return Entity(e, scene).GetComponent<IDComponent>().ID.Get();
            };

            writer.Write(GetUUID(comp.Parent), GetUUID(comp.FirstChild), GetUUID(comp.PreviousSibling), GetUUID(comp.NextSibling), comp.ChildrenCount);
        }
        else
        {
            const Byte* base = reinterpret_cast<const Byte*>(&comp);
            for(const SerializedVariable& var : GetSerializedVariables<T>())
                WriteField(writer, var.Kind, base + var.Offset);

            if constexpr(std::is_same_v<T, ScriptComponent>)
            {
                writer.Write(static_cast<Uint>(comp.Scripts.size()));
                for(const ScriptEntry& entry : comp.Scripts)
                    writer.Write(entry.Active, entry.ScriptAsset ? entry.ScriptAsset->GetID().Get() : static_cast<uint64_t>(AssetID::INVALID), entry.Fields);
            }
            else if constexpr(std::is_same_v<T, UICanvasComponent>)
                writer.Write(comp.Fields);
        }
    }

    template <typename T>
    static void ReadComponent(Rapt0R::Reader& reader, Entity& entity, Vector<PendingLinks>& links)
    {
        if(!entity.HasComponent<T>())
            entity.AddComponent<T>();

        T& comp = entity.GetComponent<T>();
        if constexpr(std::is_same_v<T, RelationshipComponent>)
        {
            PendingLinks& pending = links.emplace_back();
            pending.Entity = entity.Raw();
            reader.Read(pending.Parent, pending.FirstChild, pending.PreviousSibling, pending.NextSibling, pending.ChildrenCount);
        }
        else
        {
            AssetManager& assetManager = *entity.GetScene()->GetContext().AssetManager_;
            Byte* base = reinterpret_cast<Byte*>(&comp);
            for(const SerializedVariable& var : GetSerializedVariables<T>())
                ReadField(reader, assetManager, var.Kind, base + var.Offset);

            if constexpr(std::is_same_v<T, ScriptComponent>)
            {
                Uint count = 0;
                if(!reader.Read(count) || !reader.EnsureElements(count))
                    return;

                comp.Scripts.clear();
                for(Uint i = 0; i < count && !reader.HasError(); i++)
                {
                    ScriptEntry& entry = comp.Scripts.emplace_back();
                    uint64_t rawID = 0;
                    if(reader.Read(entry.Active, rawID, entry.Fields))
                        entry.ScriptAsset = LoadSerializedAsset(assetManager, AssetID(rawID), AssetType::SCRIPT);
                }
            }
            else if constexpr(std::is_same_v<T, UICanvasComponent>)
                reader.Read(comp.Fields);
        }
    }

    template <typename... Components>
    static Uint BuildMask(Entity& entity)
    {
        Uint mask = 0;
        Uint bit = 0;
        ((mask |= (entity.HasComponent<Components>() ? 1u : 0u) << bit++), ...);
        return mask;
    }

    template <typename... Components>
    static void WriteComponents(Rapt0R::Writer& writer, Entity& entity)
    {
        (WriteComponent<Components>(writer, entity), ...);
    }

    template <typename... Components>
    static void ReadComponents(Rapt0R::Reader& reader, Entity& entity, Uint mask, Vector<PendingLinks>& links)
    {
        Uint bit = 0;
        ((((mask >> bit++) & 1u) != 0 && !reader.HasError() ? ReadComponent<Components>(reader, entity, links) : void()), ...);
    }

    //////////////////////////////////////////////////////////////////////////
    // SceneBinary
    //////////////////////////////////////////////////////////////////////////

    void Write(Rapt0R::Writer& writer, Scene* scene)
    {
        SG_ASSERT_NOMSG(scene);

        // Same entity order as the JSON format
        Vector<entt::entity> entities;
        scene->GetRegistry().each([&entities](entt::entity e) { entities.push_back(e); });

        writer.Write(GetSchemaHash(), static_cast<Uint>(entities.size()));
        for(entt::entity e : entities)
        {
            Entity entity(e, scene);
            writer.Write(BuildMask<SERIALIZABLE_COMPONENTS>(entity));
            WriteComponents<SERIALIZABLE_COMPONENTS>(writer, entity);
        }
    }

    bool Read(Rapt0R::Reader& reader, Scene* outScene)
    {
        SG_ASSERT_NOMSG(outScene);

        Uint schemaHash = 0;
        Uint entityCount = 0;
        if(!reader.Read(schemaHash, entityCount))
            return false;
        if(schemaHash != GetSchemaHash())
        {
            Log<Severity::Error>("[SceneBinary] The scene was cooked with another component layout. Re-cook it in the Editor (or export the game again)");
            return false;
        }
        if(!reader.EnsureElements(entityCount, sizeof(Uint)))
            return false;

        entt::registry& registry = outScene->GetRegistry();
        registry.clear();

        Vector<PendingLinks> links;
        links.reserve(entityCount);
        std::unordered_map<uint64_t, entt::entity> entitiesByID;
        entitiesByID.reserve(entityCount);

        for(Uint i = 0; i < entityCount && !reader.HasError(); i++)
        {
            Uint mask = 0;
            reader.Read(mask);

            Entity entity;
            outScene->CreateEntity(entity, "");
            ReadComponents<SERIALIZABLE_COMPONENTS>(reader, entity, mask, links);
            entitiesByID[entity.GetComponent<IDComponent>().ID.Get()] = entity.Raw();
        }

        if(reader.HasError())
        {
            registry.clear();
            return false;
        }

        auto GetHandle = [&entitiesByID](uint64_t id) -> entt::entity
        {
            if(id == UUID::INVALID)
                return entt::null;
            auto it = entitiesByID.find(id);
            return it != entitiesByID.end() ? it->second : entt::null;
        };

        for(const PendingLinks& pending : links)
        {
            RelationshipComponent& rel = registry.get<RelationshipComponent>(pending.Entity);
            rel.Parent = static_cast<Uint>(GetHandle(pending.Parent));
            rel.FirstChild = static_cast<Uint>(GetHandle(pending.FirstChild));
            rel.PreviousSibling = static_cast<Uint>(GetHandle(pending.PreviousSibling));
            rel.NextSibling = static_cast<Uint>(GetHandle(pending.NextSibling));
            rel.ChildrenCount = pending.ChildrenCount;
        }
        return true;
    }
}

namespace Surge
{
    Ref<Asset> SceneFormat::Read(CookedFile& file, const AssetMetadata& metadata, const AssetContext& context) const
    {
        Ref<Scene> scene = Scene::Create(Path(metadata.RelativePath).stem().string(), context.Scenes);
        Rapt0R::Reader payload = file.GetPayload();
        if(!SceneBinary::Read(payload, scene.Raw()))
        {
            Log<Severity::Error>("[SceneFormat] {} is truncated, corrupted or stale!", metadata.RelativePath);
            return nullptr;
        }
        return scene.As<Asset>();
    }

    bool SceneFormat::Write(Rapt0R::Writer& payload, const Asset& asset) const
    {
        SceneBinary::Write(payload, const_cast<Scene*>(&static_cast<const Scene&>(asset))); // Entity access is non-const, nothing is modified
        return !payload.HasError();
    }
}
