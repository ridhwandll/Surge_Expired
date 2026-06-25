// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/ECS/Components.hpp"

// SurgeReflect - Component Register

// Must be in Gobal Namespace
// clang-format off

SURGE_REFLECT_CLASS_REGISTER_BEGIN(Surge::IDComponent)
    .AddVariable<&Surge::IDComponent::ID>("ID")
SURGE_REFLECT_CLASS_REGISTER_END()

SURGE_REFLECT_CLASS_REGISTER_BEGIN(Surge::NameComponent)
    .AddVariable<&Surge::NameComponent::Name>("Name")
SURGE_REFLECT_CLASS_REGISTER_END()

SURGE_REFLECT_CLASS_REGISTER_BEGIN(Surge::RelationshipComponent)
// (Rid) We manually serialize/deserialize this component, members are not reflected, but we still need to register the class
SURGE_REFLECT_CLASS_REGISTER_END()

SURGE_REFLECT_CLASS_REGISTER_BEGIN(Surge::TransformComponent)
    .AddVariable<&Surge::TransformComponent::Position>("Position")
    .AddVariable<&Surge::TransformComponent::Rotation>("Rotation")
    .AddVariable<&Surge::TransformComponent::Scale>("Scale")
SURGE_REFLECT_CLASS_REGISTER_END()

SURGE_REFLECT_CLASS_REGISTER_BEGIN(Surge::SpriteRendererComponent)
    .AddVariable<&Surge::SpriteRendererComponent::Color>("Color")
    .AddVariable<&Surge::SpriteRendererComponent::Billboard>("Billboard")
    .AddVariable<&Surge::SpriteRendererComponent::Texture>("Texture")
SURGE_REFLECT_CLASS_REGISTER_END()

SURGE_REFLECT_CLASS_REGISTER_BEGIN(Surge::CameraComponent)
    .AddVariable<&Surge::CameraComponent::Camera>("Camera")
    .AddVariable<&Surge::CameraComponent::Primary>("Primary")
    .AddVariable<&Surge::CameraComponent::FixedAspectRatio>("FixedAspectRatio")
SURGE_REFLECT_CLASS_REGISTER_END()

SURGE_REFLECT_CLASS_REGISTER_BEGIN(Surge::MeshComponent)
    .AddVariable<&Surge::MeshComponent::MeshID>("MeshID")
    .AddVariable<&Surge::MeshComponent::DropShadow>("DropShadow")
SURGE_REFLECT_CLASS_REGISTER_END()

SURGE_REFLECT_CLASS_REGISTER_BEGIN(Surge::LightComponent)
    .AddVariable<&Surge::LightComponent::Color>("Color")
    .AddVariable<&Surge::LightComponent::Intensity>("Intensity")
    .AddVariable<&Surge::LightComponent::Radius>("Radius")
    .AddVariable<&Surge::LightComponent::Type>("Type")
SURGE_REFLECT_CLASS_REGISTER_END()

SURGE_REFLECT_CLASS_REGISTER_BEGIN(Surge::EnvironmentComponent)
    .AddVariable<&Surge::EnvironmentComponent::Elevation>("Elevation")
    .AddVariable<&Surge::EnvironmentComponent::Azimuth>("Azimuth")
    .AddVariable<&Surge::EnvironmentComponent::Turbidity>("Turbidity")
    .AddVariable<&Surge::EnvironmentComponent::Exposure>("Exposure")
    .AddVariable<&Surge::EnvironmentComponent::SunIntensity>("SunIntensity")
    .AddVariable<&Surge::EnvironmentComponent::EnableSunDisk>("EnableSunDisk")
    .AddVariable<&Surge::EnvironmentComponent::SkyAmbient>("SkyAmbient")
    .AddVariable<&Surge::EnvironmentComponent::HorizonAmbient>("HorizonAmbient")
    .AddVariable<&Surge::EnvironmentComponent::GroundAmbient>("GroundAmbient")
SURGE_REFLECT_CLASS_REGISTER_END()

SURGE_REFLECT_CLASS_REGISTER_BEGIN(Surge::RigidbodyComponent)
    .AddVariable<&Surge::RigidbodyComponent::Type>("Type")
    .AddVariable<&Surge::RigidbodyComponent::Mass>("Mass")
    .AddVariable<&Surge::RigidbodyComponent::UseGravity>("UseGravity")
    .AddVariable<&Surge::RigidbodyComponent::IsSensor>("IsSensor")
    .AddVariable<&Surge::RigidbodyComponent::ContinuousCollision>("ContinuousCollision")
    .AddVariable<&Surge::RigidbodyComponent::FreezeRotationX>("FreezeRotationX")
    .AddVariable<&Surge::RigidbodyComponent::FreezeRotationY>("FreezeRotationY")
    .AddVariable<&Surge::RigidbodyComponent::FreezeRotationZ>("FreezeRotationZ")
    .AddVariable<&Surge::RigidbodyComponent::LinearDamping>("LinearDamping")
    .AddVariable<&Surge::RigidbodyComponent::AngularDamping>("AngularDamping")
    .AddVariable<&Surge::RigidbodyComponent::Friction>("Friction")
    .AddVariable<&Surge::RigidbodyComponent::Bounciness >("Bounciness")
SURGE_REFLECT_CLASS_REGISTER_END()

SURGE_REFLECT_CLASS_REGISTER_BEGIN(Surge::BoxColliderComponent)
    .AddVariable<&Surge::BoxColliderComponent::HalfExtents>("HalfExtents")
SURGE_REFLECT_CLASS_REGISTER_END()

SURGE_REFLECT_CLASS_REGISTER_BEGIN(Surge::SphereColliderComponent)
    .AddVariable<&Surge::SphereColliderComponent::Radius>("Radius")
SURGE_REFLECT_CLASS_REGISTER_END()

SURGE_REFLECT_CLASS_REGISTER_BEGIN(Surge::CapsuleColliderComponent)
    .AddVariable<&Surge::CapsuleColliderComponent::Height>("Height")
    .AddVariable<&Surge::CapsuleColliderComponent::Radius>("Radius")
SURGE_REFLECT_CLASS_REGISTER_END()

SURGE_REFLECT_CLASS_REGISTER_BEGIN(Surge::CylinderColliderComponent)
    .AddVariable<&Surge::CylinderColliderComponent::Height>("Height")
    .AddVariable<&Surge::CylinderColliderComponent::Radius>("Radius")
SURGE_REFLECT_CLASS_REGISTER_END()

SURGE_REFLECT_CLASS_REGISTER_BEGIN(Surge::ConvexColliderComponent)
    .AddVariable<&Surge::ConvexColliderComponent::LocalOffset>("LocalOffset")
    .AddVariable<&Surge::ConvexColliderComponent::LocalRotation>("LocalRotation")
SURGE_REFLECT_CLASS_REGISTER_END()

SURGE_REFLECT_CLASS_REGISTER_BEGIN(Surge::MeshColliderComponent)
    .AddVariable<&Surge::MeshColliderComponent::LocalOffset>("LocalOffset")
    .AddVariable<&Surge::MeshColliderComponent::LocalRotation>("LocalRotation")
SURGE_REFLECT_CLASS_REGISTER_END()

SURGE_REFLECT_CLASS_REGISTER_BEGIN(Surge::ScriptComponent)
    .AddVariable<&Surge::ScriptComponent::ScriptAsset>("ScriptAsset")
SURGE_REFLECT_CLASS_REGISTER_END()

SURGE_REFLECT_CLASS_REGISTER_BEGIN(Surge::TextComponent)
    .AddVariable<&Surge::TextComponent::Text>("Text")
    .AddVariable<&Surge::TextComponent::Color>("Color")
    .AddVariable<&Surge::TextComponent::MaxWidth>("MaxWidth")
    .AddVariable<&Surge::TextComponent::LetterSpacing>("LetterSpacing")
    .AddVariable<&Surge::TextComponent::LineSpacing>("LineSpacing")
    .AddVariable<&Surge::TextComponent::Alignment>("Alignment")
    .AddVariable<&Surge::TextComponent::Italic>("Italic")
    .AddVariable<&Surge::TextComponent::Billboard>("Billboard")
    .AddVariable<&Surge::TextComponent::Underline>("Underline")
    .AddVariable<&Surge::TextComponent::FontAssetID>("FontAssetID")
    .AddVariable<&Surge::TextComponent::ShadowEnabled>("ShadowEnabled")
    .AddVariable<&Surge::TextComponent::ShadowOffset>("ShadowOffset")
    .AddVariable<&Surge::TextComponent::ShadowColor>("ShadowColor")
SURGE_REFLECT_CLASS_REGISTER_END()
