// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Asset/Format/Scene/SerializableComponents.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Audio/Audio.hpp"
#include "Surge/ScriptEngine/ScriptAsset.hpp"
#include "Surge/Graphics/HighLevel/Font.hpp"
#include "Surge/Graphics/HighLevel/Mesh.hpp"
#include "Surge/Graphics/HighLevel/Texture2D.hpp"
#include "Surge/Graphics/UISystem/UILayout.hpp"
#include "Surge/Graphics/Particles/ParticleEffect.hpp"

namespace Surge
{
    SerializedFieldKind GetSerializedFieldKind(const SurgeReflect::Type& type)
    {
        if(type.EqualTo<bool>()) return SerializedFieldKind::BOOL;
        if(type.EqualTo<float>()) return SerializedFieldKind::FLOAT;
        if(type.EqualTo<int>()) return SerializedFieldKind::INT;
        if(type.EqualTo<UUID>() || type.EqualTo<AssetID>()) return SerializedFieldKind::UUID;
        if(type.EqualTo<String>()) return SerializedFieldKind::STRING;
        if(type.EqualTo<glm::vec2>()) return SerializedFieldKind::VEC2;
        if(type.EqualTo<glm::vec3>()) return SerializedFieldKind::VEC3;
        if(type.EqualTo<glm::vec4>()) return SerializedFieldKind::VEC4;
        if(type.EqualTo<RuntimeCamera>()) return SerializedFieldKind::RUNTIME_CAMERA;
        if(type.EqualTo<LightType>()) return SerializedFieldKind::LIGHT_TYPE;
        if(type.EqualTo<AttenuationModel>()) return SerializedFieldKind::ATTENUATION_MODEL;
        if(type.EqualTo<RigidbodyType>()) return SerializedFieldKind::RIGIDBODY_TYPE;
        if(type.EqualTo<TextAlignment>()) return SerializedFieldKind::TEXT_ALIGNMENT;
        if(type.EqualTo<TextVerticalAlignment>()) return SerializedFieldKind::TEXT_VERTICAL_ALIGNMENT;
        if(type.EqualTo<ParticleCullingMode>()) return SerializedFieldKind::PARTICLE_CULLING_MODE;
        if(type.EqualTo<Ref<Asset>>()) return SerializedFieldKind::ASSET_REF;
        if(type.EqualTo<RenderSettings>()) return SerializedFieldKind::RENDER_SETTINGS;
        return SerializedFieldKind::UNKNOWN;
    }

    Ref<Asset> LoadSerializedAsset(AssetManager& assetManager, AssetID id, AssetType requiredType)
    {
        if(id == AssetID(AssetID::INVALID))
            return nullptr;

        AssetManager* am = &assetManager;
        const AssetMetadata& metadata = am->GetMetadata(id);
        if(!metadata.IsValid() || metadata.IsMissing() || (requiredType != AssetType::NONE && metadata.Type != requiredType))
        {
            Log<Severity::Warn>("[Serializer] Asset with ID {} is missing or invalid", id.Get());
            return nullptr;
        }

        switch(metadata.Type)
        {
            case AssetType::TEXTURE2D: return am->Load<Texture2D>(id);
            case AssetType::MESH: return am->Load<Mesh>(id);
            case AssetType::FONT: return am->Load<Font>(id);
            case AssetType::SCRIPT: return am->Load<Script>(id);
            case AssetType::AUDIO: return am->Load<Audio>(id);
            case AssetType::UI_LAYOUT: return am->Load<UILayout>(id);
            case AssetType::PARTICLE_EFFECT: return am->Load<ParticleEffect>(id);
            default: return nullptr;
        }
    }

} // namespace Surge
