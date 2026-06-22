// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include "Surge/Core/UUID.hpp"
#include "Surge/Graphics/Camera/RuntimeCamera.hpp"
#include "Surge/Graphics/Renderer/Lights.hpp"
#include "Surge/Physics/RigidbodyID.hpp"
#include "Surge/Asset/Asset.hpp"
#include "SurgeReflect/SurgeReflect.hpp"
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

    struct RelationshipComponent
    {
        #define RELATIONSHIP_NULL 0xFFFFFFFF

        // Uint is the underlying type of entt::entity, we use this to avoid including entt in this header
        Uint Parent = RELATIONSHIP_NULL;
        Uint FirstChild = RELATIONSHIP_NULL;
        Uint PreviousSibling = RELATIONSHIP_NULL;
        Uint NextSibling = RELATIONSHIP_NULL;

        Uint ChildrenCount = 0;
        SURGE_REFLECTION_ENABLE;
    };

    struct NameComponent
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

        void MarkDirty() { mDirty = true; }
        bool IsDirty() const { return mDirty; }

        const glm::mat4& GetTransform() const
        {
            // For an entity with no parent, World = Local
            if(mDirty)
                WorldTransform = GetLocalTransform();

            return WorldTransform;
        }

        const glm::mat4& GetLocalTransform() const
        {
            if(mDirty)
            {
                LocalTransform = glm::translate(glm::mat4(1.0f), Position) * glm::mat4_cast(glm::quat(glm::radians(Rotation))) * glm::scale(glm::mat4(1.0f), Scale);
                mDirty = false;
            }
            return LocalTransform;
        }

        // Set by Scene.cpp when propagating parent transforms down the hierarchy
        void SetWorldTransform(const glm::mat4& transform) { WorldTransform = transform; }

        SURGE_REFLECTION_ENABLE;
    private:
        mutable glm::mat4 LocalTransform { 1.0f };
        mutable glm::mat4 WorldTransform { 1.0f };
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
        AssetID Texture = AssetID::INVALID;

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
            : Color(color), Intensity(intensity), Radius(radius), Falloff(falloff), Type(type) {}

        glm::vec3 Color = { 1.0f, 1.0f, 1.0f };
        float Intensity = 1.0f;
        float Radius = 3.0f;
        float Falloff = 1.0f;
        LightType Type = LightType::POINT;
        SURGE_REFLECTION_ENABLE;
    };

    struct EnvironmentComponent
    {
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
        RigidBodyID RuntimeBodyID; // Jolt's internal handle under the hood
        RigidbodyType Type = RigidbodyType::DYNAMIC;
        float Mass = 1.0f;

        bool UseGravity = true;
        bool IsSensor = false;
        bool ContinuousCollision = false;

        bool FreezeRotationX = false;
        bool FreezeRotationY = false;
        bool FreezeRotationZ = false;

        float LinearDamping = 0.05f;
        float AngularDamping = 0.05f;
        float Friction = 0.2f;
        float Bounciness = 0.0f;

        SURGE_REFLECTION_ENABLE;
    };

    struct BoxColliderComponent
    {
        bool ShowCollider = true;
        glm::vec3 HalfExtents = { 0.5f, 0.5f, 0.5f }; // A 1x1x1 meter cube
        SURGE_REFLECTION_ENABLE;
    };

    struct SphereColliderComponent
    {
        bool ShowCollider = true;
        float Radius = 0.5f;
        SURGE_REFLECTION_ENABLE;
    };

    struct CapsuleColliderComponent
    {
        bool ShowCollider = true;
        float Height = 1.0f;
        float Radius = 0.25f;
        SURGE_REFLECTION_ENABLE;
    };

    struct CylinderColliderComponent
    {
        bool ShowCollider = true;
        float Height = 1.0f;
        float Radius = 0.5f;
        SURGE_REFLECTION_ENABLE;
    };

    struct ConvexColliderComponent
    {
        glm::vec3 LocalOffset = { 0.0f, 0.0f, 0.0f };
        glm::vec3 LocalRotation = { 0.0f, 0.0f, 0.0f }; // In degrees
        // Uses the entity's MeshComponent to generate the hull

        // Internal usage, not serialized
        bool ShowCollider = false;
        bool IsDirty = true;

        SURGE_REFLECTION_ENABLE;
    };

    struct MeshColliderComponent
    {
        glm::vec3 LocalOffset = { 0.0f, 0.0f, 0.0f };
        glm::vec3 LocalRotation = { 0.0f, 0.0f, 0.0f };
        SURGE_REFLECTION_ENABLE;
    };

    struct ScriptComponent
    {
        AssetID ScriptAsset = AssetID::INVALID;
        bool IsInstantiated = false;
        SURGE_REFLECTION_ENABLE;
    };

} // namespace Surge

//! NOTE: ALL THE SERIALIZABLE COMPONENTS MUST BE REGISTERED HERE, ADD BY SEPARATING VIA A COMMA (',') WHEN YOU ADD A NEW COMPONENT
#define SERIALIZABLE_COMPONENTS ::Surge::IDComponent,        ::Surge::NameComponent,          ::Surge::TransformComponent,      \
                                ::Surge::CameraComponent,    ::Surge::SpriteRendererComponent, ::Surge::MeshComponent, ::Surge::RelationshipComponent, \
                                ::Surge::LightComponent,     ::Surge::EnvironmentComponent, \
                                ::Surge::RigidbodyComponent, ::Surge::BoxColliderComponent, ::Surge::SphereColliderComponent, \
                                ::Surge::CapsuleColliderComponent, ::Surge::CylinderColliderComponent, ::Surge::ConvexColliderComponent, ::Surge::MeshColliderComponent, \
                                ::Surge::ScriptComponent
