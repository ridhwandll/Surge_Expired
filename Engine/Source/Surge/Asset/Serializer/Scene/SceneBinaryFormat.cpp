// Copyright (c) - SurgeTechnologies - All rights reserved
#include "SceneBinaryFormat.hpp"
#include "Surge/Asset/Serializer/BinaryHelpers.hpp"
#include "Surge/Asset/Serializer/Scene/SerializableComponents.hpp"
#include "Surge/ECS/Scene.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include <unordered_map>
#include <cstring>

namespace Surge::SceneBinary
{
    static constexpr Uint MAGIC = 0x53444952; // RIDS
    static constexpr Uint VERSION = 1;

    struct Header
    {
        Uint Magic;
        Uint Version;
        Uint SchemaHash;
        Uint EntityCount;
    };

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

    Uint GetFormatVersion()
    {
        Uint hash = GetSchemaHash();
        HashBytes(hash, &VERSION, sizeof(VERSION));
        return hash;
    }

    //////////////////////////////////////////////////////////////////////////
    // Fields
    //////////////////////////////////////////////////////////////////////////

    template <typename E>
    static void WriteEnum(Vector<Byte>& buffer, const Byte* src)
    {
        WriteData(buffer, static_cast<int32_t>(*reinterpret_cast<const E*>(src)));
    }

    template <typename E>
    static void ReadEnum(BinaryReader& reader, Byte* dest)
    {
        int32_t value = 0;
        reader.Read(value);
        *reinterpret_cast<E*>(dest) = static_cast<E>(value);
    }

    static void WriteBool(Vector<Byte>& buffer, bool value)
    {
        WriteData(buffer, static_cast<Byte>(value ? 1 : 0));
    }

    static bool ReadBool(BinaryReader& reader)
    {
        Byte value = 0;
        reader.Read(value);
        return value != 0;
    }

    template <typename T>
    static void WriteSettingsField(Vector<Byte>& buffer, const T& value)
    {
        if constexpr(std::is_same_v<T, bool>)
            WriteBool(buffer, value);
        else if constexpr(std::is_enum_v<T>)
            WriteData(buffer, static_cast<int32_t>(value));
        else
            WriteData(buffer, value); // int, Uint, float, glm::vec3
    }

    template <typename T>
    static void ReadSettingsField(BinaryReader& reader, T& value)
    {
        if constexpr(std::is_same_v<T, bool>)
            value = ReadBool(reader);
        else if constexpr(std::is_enum_v<T>)
        {
            int32_t raw = 0;
            reader.Read(raw);
            value = static_cast<T>(raw);
        }
        else
            reader.Read(value);
    }

    static void WriteField(Vector<Byte>& buffer, SerializedFieldKind kind, const Byte* src)
    {
        switch(kind)
        {
            case SerializedFieldKind::BOOL: WriteBool(buffer, *reinterpret_cast<const bool*>(src)); break;
            case SerializedFieldKind::FLOAT: WriteData(buffer, *reinterpret_cast<const float*>(src)); break;
            case SerializedFieldKind::INT: WriteData(buffer, static_cast<int32_t>(*reinterpret_cast<const int*>(src))); break;
            case SerializedFieldKind::UUID: WriteData(buffer, *reinterpret_cast<const uint64_t*>(src)); break;
            case SerializedFieldKind::STRING: WriteStr(buffer, *reinterpret_cast<const String*>(src)); break;
            case SerializedFieldKind::VEC2: WriteData(buffer, *reinterpret_cast<const glm::vec2*>(src)); break;
            case SerializedFieldKind::VEC3: WriteData(buffer, *reinterpret_cast<const glm::vec3*>(src)); break;
            case SerializedFieldKind::VEC4: WriteData(buffer, *reinterpret_cast<const glm::vec4*>(src)); break;
            case SerializedFieldKind::RUNTIME_CAMERA:
            {
                const RuntimeCamera* cam = reinterpret_cast<const RuntimeCamera*>(src);
                WriteData(buffer, cam->GetPerspectiveVerticalFOV());
                WriteData(buffer, cam->GetPerspectiveNearClip());
                WriteData(buffer, cam->GetPerspectiveFarClip());
                WriteData(buffer, cam->GetOrthographicNearClip());
                WriteData(buffer, cam->GetOrthographicFarClip());
                WriteData(buffer, cam->GetOrthographicSize());
                WriteData(buffer, static_cast<int32_t>(cam->GetProjectionType()));
                break;
            }
            case SerializedFieldKind::LIGHT_TYPE: WriteEnum<LightType>(buffer, src); break;
            case SerializedFieldKind::ATTENUATION_MODEL: WriteEnum<AttenuationModel>(buffer, src); break;
            case SerializedFieldKind::RIGIDBODY_TYPE: WriteEnum<RigidbodyType>(buffer, src); break;
            case SerializedFieldKind::TEXT_ALIGNMENT: WriteEnum<TextAlignment>(buffer, src); break;
            case SerializedFieldKind::TEXT_VERTICAL_ALIGNMENT: WriteEnum<TextVerticalAlignment>(buffer, src); break;
            case SerializedFieldKind::PARTICLE_CULLING_MODE: WriteEnum<ParticleCullingMode>(buffer, src); break;
            case SerializedFieldKind::ASSET_REF:
            {
                const Ref<Asset>& asset = *reinterpret_cast<const Ref<Asset>*>(src);
                WriteData(buffer, asset ? asset->GetID().Get() : static_cast<uint64_t>(AssetID::INVALID));
                break;
            }
            case SerializedFieldKind::RENDER_SETTINGS:
                RenderSettings::VisitSavedFields(*reinterpret_cast<const RenderSettings*>(src), [&buffer](const char*, const char*, const auto& value) { WriteSettingsField(buffer, value); });
                break;
            case SerializedFieldKind::UNKNOWN: break; // Not saved, the JSON format skips it too
        }
    }

    static void ReadField(BinaryReader& reader, AssetManager& assetManager, SerializedFieldKind kind, Byte* dest)
    {
        switch(kind)
        {
            case SerializedFieldKind::BOOL: *reinterpret_cast<bool*>(dest) = ReadBool(reader); break;
            case SerializedFieldKind::FLOAT: reader.Read(*reinterpret_cast<float*>(dest)); break;
            case SerializedFieldKind::INT:
            {
                int32_t value = 0;
                reader.Read(value);
                *reinterpret_cast<int*>(dest) = value;
                break;
            }
            case SerializedFieldKind::UUID: reader.Read(*reinterpret_cast<uint64_t*>(dest)); break;
            case SerializedFieldKind::STRING: *reinterpret_cast<String*>(dest) = reader.ReadStr(); break;
            case SerializedFieldKind::VEC2: reader.Read(*reinterpret_cast<glm::vec2*>(dest)); break;
            case SerializedFieldKind::VEC3: reader.Read(*reinterpret_cast<glm::vec3*>(dest)); break;
            case SerializedFieldKind::VEC4: reader.Read(*reinterpret_cast<glm::vec4*>(dest)); break;
            case SerializedFieldKind::RUNTIME_CAMERA:
            {
                float values[6] = {};
                int32_t projection = 0;
                reader.ReadArray(values, 6);
                reader.Read(projection);
                if(reader.Failed)
                    break;

                RuntimeCamera* cam = reinterpret_cast<RuntimeCamera*>(dest);
                cam->SetPerspectiveVerticalFOV(values[0]);
                cam->SetPerspectiveNearClip(values[1]);
                cam->SetPerspectiveFarClip(values[2]);
                cam->SetOrthographicNearClip(values[3]);
                cam->SetOrthographicFarClip(values[4]);
                cam->SetOrthographicSize(values[5]);
                cam->SetProjectionType(static_cast<RuntimeCamera::ProjectionType>(projection));
                break;
            }
            case SerializedFieldKind::LIGHT_TYPE: ReadEnum<LightType>(reader, dest); break;
            case SerializedFieldKind::ATTENUATION_MODEL: ReadEnum<AttenuationModel>(reader, dest); break;
            case SerializedFieldKind::RIGIDBODY_TYPE: ReadEnum<RigidbodyType>(reader, dest); break;
            case SerializedFieldKind::TEXT_ALIGNMENT: ReadEnum<TextAlignment>(reader, dest); break;
            case SerializedFieldKind::TEXT_VERTICAL_ALIGNMENT: ReadEnum<TextVerticalAlignment>(reader, dest); break;
            case SerializedFieldKind::PARTICLE_CULLING_MODE: ReadEnum<ParticleCullingMode>(reader, dest); break;
            case SerializedFieldKind::ASSET_REF:
            {
                uint64_t rawID = 0;
                reader.Read(rawID);
                *reinterpret_cast<Ref<Asset>*>(dest) = reader.Failed ? nullptr : LoadSerializedAsset(assetManager, AssetID(rawID));
                break;
            }
            case SerializedFieldKind::RENDER_SETTINGS:
                RenderSettings::VisitSavedFields(*reinterpret_cast<RenderSettings*>(dest), [&reader](const char*, const char*, auto& value) { ReadSettingsField(reader, value); });
                break;
            case SerializedFieldKind::UNKNOWN: break;
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // Script fields
    //////////////////////////////////////////////////////////////////////////

    static void WriteFieldValue(Vector<Byte>& buffer, ScriptFieldType type, ScriptFieldType elementType, const ScriptFieldValue& value)
    {
        switch(type)
        {
            case ScriptFieldType::FLOAT:
            case ScriptFieldType::INT: WriteData(buffer, value.Number); break;
            case ScriptFieldType::BOOL: WriteBool(buffer, value.Bool); break;
            case ScriptFieldType::STRING:
            case ScriptFieldType::ENUM: WriteStr(buffer, value.Text); break;
            case ScriptFieldType::VEC2:
            case ScriptFieldType::VEC3:
            case ScriptFieldType::VEC4:
            case ScriptFieldType::COLOR: WriteData(buffer, value.Vector); break;
            case ScriptFieldType::ENTITY: WriteData(buffer, value.EntityID); break;
            case ScriptFieldType::ASSET: WriteData(buffer, value.AssetID); break;
            case ScriptFieldType::ARRAY:
            {
                WriteData(buffer, static_cast<Uint>(value.Elements.size()));
                for(const ScriptFieldValue& element : value.Elements)
                    WriteFieldValue(buffer, elementType, ScriptFieldType::FLOAT, element);
                break;
            }
            default: break;
        }
    }

    static void ReadFieldValue(BinaryReader& reader, ScriptFieldType type, ScriptFieldType elementType, ScriptFieldValue& out)
    {
        switch(type)
        {
            case ScriptFieldType::FLOAT:
            case ScriptFieldType::INT: reader.Read(out.Number); break;
            case ScriptFieldType::BOOL: out.Bool = ReadBool(reader); break;
            case ScriptFieldType::STRING:
            case ScriptFieldType::ENUM: out.Text = reader.ReadStr(); break;
            case ScriptFieldType::VEC2:
            case ScriptFieldType::VEC3:
            case ScriptFieldType::VEC4:
            case ScriptFieldType::COLOR: reader.Read(out.Vector); break;
            case ScriptFieldType::ENTITY: reader.Read(out.EntityID); break;
            case ScriptFieldType::ASSET: reader.Read(out.AssetID); break;
            case ScriptFieldType::ARRAY:
            {
                Uint count = 0;
                reader.Read(count);
                if(!reader.Fits(count, 1))
                {
                    reader.Failed = true;
                    break;
                }

                out.Elements.resize(count);
                for(Uint i = 0; i < count && !reader.Failed; i++)
                    ReadFieldValue(reader, elementType, ScriptFieldType::FLOAT, out.Elements[i]);
                break;
            }
            default: break;
        }
    }

    static void WriteFieldOverrides(Vector<Byte>& buffer, const Vector<ScriptFieldOverride>& fields)
    {
        WriteData(buffer, static_cast<Uint>(fields.size()));
        for(const ScriptFieldOverride& field : fields)
        {
            WriteStr(buffer, field.Name);
            WriteData(buffer, static_cast<Byte>(field.Type));
            WriteData(buffer, static_cast<Byte>(field.ElementType));
            WriteFieldValue(buffer, field.Type, field.ElementType, field.Value);
        }
    }

    static void ReadFieldOverrides(BinaryReader& reader, Vector<ScriptFieldOverride>& outFields)
    {
        outFields.clear();
        Uint count = 0;
        reader.Read(count);
        if(!reader.Fits(count, 1))
        {
            reader.Failed = true;
            return;
        }

        outFields.reserve(count);
        for(Uint i = 0; i < count && !reader.Failed; i++)
        {
            ScriptFieldOverride& field = outFields.emplace_back();
            field.Name = reader.ReadStr();

            Byte type = 0;
            Byte elementType = 0;
            reader.Read(type);
            reader.Read(elementType);
            if(type >= static_cast<Byte>(ScriptFieldType::COUNT) || elementType >= static_cast<Byte>(ScriptFieldType::COUNT))
            {
                reader.Failed = true;
                break;
            }

            field.Type = static_cast<ScriptFieldType>(type);
            field.ElementType = static_cast<ScriptFieldType>(elementType);
            ReadFieldValue(reader, field.Type, field.ElementType, field.Value);
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // Components
    //////////////////////////////////////////////////////////////////////////

    template <typename T>
    static void WriteComponent(Vector<Byte>& buffer, Entity& entity)
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

            WriteData(buffer, GetUUID(comp.Parent));
            WriteData(buffer, GetUUID(comp.FirstChild));
            WriteData(buffer, GetUUID(comp.PreviousSibling));
            WriteData(buffer, GetUUID(comp.NextSibling));
            WriteData(buffer, comp.ChildrenCount);
        }
        else
        {
            const Byte* base = reinterpret_cast<const Byte*>(&comp);
            for(const SerializedVariable& var : GetSerializedVariables<T>())
                WriteField(buffer, var.Kind, base + var.Offset);

            if constexpr(std::is_same_v<T, ScriptComponent>)
            {
                WriteData(buffer, static_cast<Uint>(comp.Scripts.size()));
                for(const ScriptEntry& entry : comp.Scripts)
                {
                    WriteBool(buffer, entry.Active);
                    WriteData(buffer, entry.ScriptAsset ? entry.ScriptAsset->GetID().Get() : static_cast<uint64_t>(AssetID::INVALID));
                    WriteFieldOverrides(buffer, entry.Fields);
                }
            }
            else if constexpr(std::is_same_v<T, UICanvasComponent>)
                WriteFieldOverrides(buffer, comp.Fields);
        }
    }

    template <typename T>
    static void ReadComponent(BinaryReader& reader, Entity& entity, Vector<PendingLinks>& links)
    {
        if(!entity.HasComponent<T>())
            entity.AddComponent<T>();

        T& comp = entity.GetComponent<T>();
        if constexpr(std::is_same_v<T, RelationshipComponent>)
        {
            PendingLinks& pending = links.emplace_back();
            pending.Entity = entity.Raw();
            reader.Read(pending.Parent);
            reader.Read(pending.FirstChild);
            reader.Read(pending.PreviousSibling);
            reader.Read(pending.NextSibling);
            reader.Read(pending.ChildrenCount);
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
                reader.Read(count);
                if(!reader.Fits(count, 1))
                {
                    reader.Failed = true;
                    return;
                }

                comp.Scripts.clear();
                for(Uint i = 0; i < count && !reader.Failed; i++)
                {
                    ScriptEntry& entry = comp.Scripts.emplace_back();
                    entry.Active = ReadBool(reader);
                    uint64_t rawID = 0;
                    reader.Read(rawID);
                    entry.ScriptAsset = reader.Failed ? nullptr : LoadSerializedAsset(assetManager, AssetID(rawID), AssetType::SCRIPT);
                    ReadFieldOverrides(reader, entry.Fields);
                }
            }
            else if constexpr(std::is_same_v<T, UICanvasComponent>)
                ReadFieldOverrides(reader, comp.Fields);
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
    static void WriteComponents(Vector<Byte>& buffer, Entity& entity)
    {
        (WriteComponent<Components>(buffer, entity), ...);
    }

    template <typename... Components>
    static void ReadComponents(BinaryReader& reader, Entity& entity, Uint mask, Vector<PendingLinks>& links)
    {
        Uint bit = 0;
        ((((mask >> bit++) & 1u) != 0 && !reader.Failed ? ReadComponent<Components>(reader, entity, links) : void()), ...);
    }

    //////////////////////////////////////////////////////////////////////////
    // SceneBinary
    //////////////////////////////////////////////////////////////////////////

    bool Write(const String& path, const AssetStamp& stamp, Scene* scene)
    {
        SG_ASSERT_NOMSG(scene);

        // Same entity order as the JSON format
        Vector<entt::entity> entities;
        scene->GetRegistry().each([&entities](entt::entity e) { entities.push_back(e); });

        Vector<Byte> buffer;
        buffer.reserve(sizeof(AssetStamp) + sizeof(Header) + entities.size() * 256);
        WriteData(buffer, stamp);

        const Header header = { MAGIC, VERSION, GetSchemaHash(), static_cast<Uint>(entities.size()) };
        WriteData(buffer, header);

        for(entt::entity e : entities)
        {
            Entity entity(e, scene);
            WriteData(buffer, BuildMask<SERIALIZABLE_COMPONENTS>(entity));
            WriteComponents<SERIALIZABLE_COMPONENTS>(buffer, entity);
        }

        if(!Filesystem::WriteBinaryFile(path, buffer.data(), buffer.size()))
        {
            Log<Severity::Error>("[SceneBinary] Failed to write {}", path);
            return false;
        }
        Log<Severity::Trace>("[SceneBinary] Cooked {} ({} entities)", path, entities.size());
        return true;
    }

    bool Read(const String& path, AssetStamp& outStamp, Scene* outScene)
    {
        SG_ASSERT_NOMSG(outScene);

        Vector<Byte> fileData;
        if(!Filesystem::ReadBinaryFile(path, fileData))
        {
            Log<Severity::Error>("[SceneBinary] Failed to read {}", path);
            return false;
        }

        BinaryReader reader(fileData.data(), fileData.size());
        reader.Read(outStamp);

        Header header = {};
        reader.Read(header);
        if(reader.Failed || header.Magic != MAGIC)
        {
            Log<Severity::Error>("[SceneBinary] {} is not a cooked scene!", path);
            return false;
        }
        if(header.Version != VERSION || header.SchemaHash != GetSchemaHash())
        {
            Log<Severity::Error>("[SceneBinary] {} was cooked with another component layout. Re-cook it in the Editor (or export the game again)", path);
            return false;
        }
        if(!reader.Fits(header.EntityCount, sizeof(Uint)))
        {
            Log<Severity::Error>("[SceneBinary] {} is truncated or corrupted!", path);
            return false;
        }

        entt::registry& registry = outScene->GetRegistry();
        registry.clear();

        Vector<PendingLinks> links;
        links.reserve(header.EntityCount);
        std::unordered_map<uint64_t, entt::entity> entitiesByID;
        entitiesByID.reserve(header.EntityCount);

        for(Uint i = 0; i < header.EntityCount && !reader.Failed; i++)
        {
            Uint mask = 0;
            reader.Read(mask);

            Entity entity;
            outScene->CreateEntity(entity, "");
            ReadComponents<SERIALIZABLE_COMPONENTS>(reader, entity, mask, links);
            entitiesByID[entity.GetComponent<IDComponent>().ID.Get()] = entity.Raw();
        }

        if(reader.Failed)
        {
            Log<Severity::Error>("[SceneBinary] {} is truncated or corrupted!", path);
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
