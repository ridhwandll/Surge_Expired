// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include "Surge/Core/UUID.hpp"
#include "Surge/Graphics/Camera/RuntimeCamera.hpp"
#include "Surge/Graphics/HighLevel/Material.hpp"
#include "Surge/Graphics/Renderer/Lights.hpp"
#include "SurgeReflect/SurgeReflect.hpp"
#include "Surge/Asset/Asset.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Surge
{
    struct IDComponent
    {
        IDComponent() = default;
        IDComponent(const UUID& id)
            : ID(id) {}

        UUID ID;

        SURGE_REFLECTION_ENABLE;
    };

    struct SURGE_API NameComponent
    {
        NameComponent() = default;
        NameComponent(const String& name)
            : Name(name) {}

        String Name;

        SURGE_REFLECTION_ENABLE;
    };

    struct TransformComponent
    {
        TransformComponent() = default;
        TransformComponent(const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& scale)
            : Position(position), Rotation(rotation), Scale(scale) {}

        glm::vec3 Position = glm::vec3(0.0f);
        glm::vec3 Rotation = glm::vec3(0.0f); // Degrees
        glm::vec3 Scale = glm::vec3(1.0f);

        // Call transform changed (e.g. after physics)
        void MarkDirty() { mDirty = true; }

        const glm::mat4& GetTransform() const
        {
            if (mDirty)
            {
                mCachedTransform = glm::translate(glm::mat4(1.0f), Position)
                    * glm::mat4_cast(glm::quat(glm::radians(Rotation)))
                    * glm::scale(glm::mat4(1.0f), Scale);

                mDirty = false;
            }
            return mCachedTransform;
        }

        SURGE_REFLECTION_ENABLE;

    private:
        mutable glm::mat4 mCachedTransform{ 1.0f };
        mutable bool mDirty = true;
    };

    struct SpriteRendererComponent
    {
        SpriteRendererComponent() = default;
        SpriteRendererComponent(const glm::vec4& color)
            : Color(color) {}
        SpriteRendererComponent(const glm::vec3& color, float alpha)
            : Color(glm::vec4(color, alpha)){}

        glm::vec4 Color;

        SURGE_REFLECTION_ENABLE;
    };

    struct CameraComponent
    {
        CameraComponent() = default;
        CameraComponent(const RuntimeCamera& cam, bool primary, bool fixedAspectRatio)
            : Camera(cam), Primary(primary), FixedAspectRatio(fixedAspectRatio) {}

        RuntimeCamera Camera;
        bool Primary = true;
        bool FixedAspectRatio = false;

        SURGE_REFLECTION_ENABLE;
    };

    struct MeshComponent
    {
        AssetID MeshID = AssetID::INVALID;
        bool DropShadow = true;
        SURGE_REFLECTION_ENABLE;
    };

    struct LightComponent
    {
        LightComponent() = default;
        LightComponent(LightType type, const glm::vec3& color, float intensity, float radius, float falloff)
            : Type(type), Color(color), Intensity(intensity), Radius(radius) {}

        glm::vec3 Color = { 1.0f, 1.0f, 1.0f };
        float Intensity = 1.0f;
        float Radius = 3.0f;
        float Falloff = 1.0f;
        LightType Type = LightType::POINT;
        SURGE_REFLECTION_ENABLE;
    };

    struct EnvironmentComponent
    {
        EnvironmentComponent() = default;
        EnvironmentComponent(const EnvironmentComponent&) = default;

        float Elevation = 30.0f; // In degrees
        float Azimuth = 0.0f;   // In degrees
        float Turbidity = 2.0f;
        float Exposure = 0.02f;
        float SunIntensity = 5.0f;
        bool EnableSunDisk = true;

        glm::vec3 SkyAmbient { 0.35f, 0.55f, 0.90f };
        glm::vec3 HorizonAmbient { 0.45f, 0.52f, 0.60f };
        glm::vec3 GroundAmbient { 0.12f, 0.11f, 0.10f };

        SURGE_REFLECTION_ENABLE;
    };

    //Physics
    enum class RigidbodyType { STATIC, DYNAMIC, KINEMATIC };

    struct RigidbodyComponent
    {
        JPH::BodyID RuntimeBodyID; // Jolt's internal handle
        RigidbodyType Type = RigidbodyType::DYNAMIC;
        float Mass = 1.0f;

        SURGE_REFLECTION_ENABLE;
    };

    struct BoxColliderComponent
    {
        glm::vec3 HalfExtents = { 0.5f, 0.5f, 0.5f }; // A 1x1x1 meter cube
        SURGE_REFLECTION_ENABLE;
    };

    struct SphereColliderComponent
    {
        float Radius = 0.5f;
        SURGE_REFLECTION_ENABLE;
    };

    struct CapsuleColliderComponent
    {
        float Height = 1.0f;
        float Radius = 0.25f;
        SURGE_REFLECTION_ENABLE;
    };

//! NOTE: ALL THE SERIALIZABLE COMPONENTS MUST BE REGISTERED HERE, ADD BY SEPARATING VIA A COMMA (',') WHEN YOU ADD A NEW COMPONENT
#define SERIALIZABLE_COMPONENTS ::Surge::IDComponent,        ::Surge::NameComponent,          ::Surge::TransformComponent,      \
                                ::Surge::CameraComponent,    ::Surge::SpriteRendererComponent, ::Surge::MeshComponent,     \
                                ::Surge::LightComponent,     ::Surge::EnvironmentComponent, \
                                ::Surge::RigidbodyComponent, ::Surge::BoxColliderComponent, ::Surge::SphereColliderComponent, \
                                ::Surge::CapsuleColliderComponent \

} // namespace Surge
