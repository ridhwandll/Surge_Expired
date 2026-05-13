// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/ECS/Components.hpp"

// SurgeReflect - Component Register

// Must be in Gobal Namespace
// clang-format off

SURGE_REFLECT_CLASS_REGISTER_BEGIN(Surge::IDComponent)
    .AddVariable<&Surge::IDComponent::ID>("ID")
SURGE_REFLECT_CLASS_REGISTER_END(Surge::IDComponent)

SURGE_REFLECT_CLASS_REGISTER_BEGIN(Surge::NameComponent)
    .AddVariable<&Surge::NameComponent::Name>("Name")
SURGE_REFLECT_CLASS_REGISTER_END(Surge::NameComponent)

SURGE_REFLECT_CLASS_REGISTER_BEGIN(Surge::TransformComponent)
    .AddVariable<&Surge::TransformComponent::Position>("Position")
    .AddVariable<&Surge::TransformComponent::Rotation>("Rotation")
    .AddVariable<&Surge::TransformComponent::Scale>("Scale")
SURGE_REFLECT_CLASS_REGISTER_END(Surge::TransformComponent)

SURGE_REFLECT_CLASS_REGISTER_BEGIN(Surge::SpriteRenderer)
    .AddVariable<&Surge::SpriteRenderer::Color>("Color")
SURGE_REFLECT_CLASS_REGISTER_END(Surge::SpriteRenderer)

SURGE_REFLECT_CLASS_REGISTER_BEGIN(Surge::CameraComponent)
    .AddVariable<&Surge::CameraComponent::Camera>("Camera")
    .AddVariable<&Surge::CameraComponent::Primary>("Primary")
    .AddVariable<&Surge::CameraComponent::FixedAspectRatio>("FixedAspectRatio")
SURGE_REFLECT_CLASS_REGISTER_END(Surge::CameraComponent)

SURGE_REFLECT_CLASS_REGISTER_BEGIN(Surge::MeshComponent)
    .AddVariable<&Surge::MeshComponent::Mesh>("Mesh")
SURGE_REFLECT_CLASS_REGISTER_END(Surge::MeshComponent)

SURGE_REFLECT_CLASS_REGISTER_BEGIN(Surge::PointLightComponent)
    .AddVariable<&Surge::PointLightComponent::Color>("Color")
    .AddVariable<&Surge::PointLightComponent::Intensity>("Intensity")
    .AddVariable<&Surge::PointLightComponent::Radius>("Radius")
    .AddVariable<&Surge::PointLightComponent::Falloff>("Falloff")
SURGE_REFLECT_CLASS_REGISTER_END(Surge::PointLightComponent)

SURGE_REFLECT_CLASS_REGISTER_BEGIN(Surge::DirectionalLightComponent)
    .AddVariable<&Surge::DirectionalLightComponent::Direction>("Direction")
    .AddVariable<&Surge::DirectionalLightComponent::Color>("Color")
    .AddVariable<&Surge::DirectionalLightComponent::Intensity>("Intensity")
    .AddVariable<&Surge::DirectionalLightComponent::Size>("Size")
SURGE_REFLECT_CLASS_REGISTER_END(Surge::DirectionalLightComponent)
