// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/ECS/Components/Components.hpp"
#include "Surge/ECS/Components/ScriptComponent.hpp"
#include "Surge/Asset/Asset.hpp"
#include <algorithm>

//! NOTE: ALL THE SERIALIZABLE COMPONENTS MUST BE REGISTERED HERE, ADD BY SEPARATING VIA A COMMA (',') WHEN YOU ADD A NEW COMPONENT
// Shared by the JSON scene format (SceneSerializer.cpp) and the cooked one (SceneBinaryFormat.cpp): the cooked format stores a bit per entry, so
// appending keeps it compatible, reordering changes the schema hash (every scene is re-cooked)
#define SERIALIZABLE_COMPONENTS ::Surge::IDComponent,        ::Surge::NameComponent,          ::Surge::TransformComponent,      \
                                ::Surge::CameraComponent,    ::Surge::SpriteRendererComponent, ::Surge::MeshComponent, ::Surge::RelationshipComponent, \
                                ::Surge::LightComponent,     ::Surge::EnvironmentComponent, \
                                ::Surge::RigidbodyComponent, ::Surge::BoxColliderComponent, ::Surge::SphereColliderComponent, \
                                ::Surge::CapsuleColliderComponent, ::Surge::CylinderColliderComponent, ::Surge::ConvexColliderComponent, ::Surge::MeshColliderComponent, \
                                ::Surge::ScriptComponent, ::Surge::TextComponent, ::Surge::UICanvasComponent, \
                                ::Surge::AudioListenerComponent, ::Surge::AudioSourceComponent, ::Surge::ParticleSystemComponent, \
                                ::Surge::AnimatorComponent, ::Surge::RendererSettingsComponent

namespace Surge
{
    // What a reflected component variable is saved as. Compiler independent on purpose: reflection type names differ between MSVC and clang,
    // the cooked scene format hashes these instead
    enum class SerializedFieldKind : uint8_t
    {
        UNKNOWN = 0,
        BOOL,
        FLOAT,
        INT,
        UUID,   // UUID and AssetID
        STRING,
        VEC2,
        VEC3,
        VEC4,
        RUNTIME_CAMERA,
        LIGHT_TYPE,
        ATTENUATION_MODEL,
        RIGIDBODY_TYPE,
        TEXT_ALIGNMENT,
        TEXT_VERTICAL_ALIGNMENT,
        PARTICLE_CULLING_MODE,
        ASSET_REF, // Ref<Asset>, saved as its AssetID
        RENDER_SETTINGS // RenderSettings, every field of RenderSettings::VisitSavedFields
    };

    struct SerializedVariable
    {
        const String* Name = nullptr;
        uint64_t Offset = 0;
        SerializedFieldKind Kind = SerializedFieldKind::UNKNOWN;
    };

    SerializedFieldKind GetSerializedFieldKind(const SurgeReflect::Type& type);

    // Reflected variables of a component in declaration order (reflection keeps them in an unordered_map, whose order differs per standard library)
    template <typename T>
    const Vector<SerializedVariable>& GetSerializedVariables()
    {
        static const Vector<SerializedVariable> sVariables = []()
        {
            Vector<SerializedVariable> variables;
            const SurgeReflect::Class* clazz = SurgeReflect::GetReflection<T>();
            variables.reserve(clazz->GetVariables().size());
            for(const auto& [name, var] : clazz->GetVariables())
                variables.push_back({ &name, var.GetOffset(), GetSerializedFieldKind(var.GetType()) });

            std::sort(variables.begin(), variables.end(), [](const SerializedVariable& a, const SerializedVariable& b) { return a.Offset < b.Offset; });
            return variables;
        }();
        return sVariables;
    }

    // Loads a Ref<Asset> field saved by ID. Null for an invalid ID, a missing asset, or when requiredType is set and does not match
    class AssetManager;
    Ref<Asset> LoadSerializedAsset(AssetManager& assetManager, AssetID id, AssetType requiredType = AssetType::NONE);

} // namespace Surge
