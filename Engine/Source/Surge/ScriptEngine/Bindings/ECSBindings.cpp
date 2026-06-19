// Copyright (c) - SurgeTechnologies - All rights reserved
#include "ECSBindings.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/ScriptEngine/Lua.hpp"
#include "Surge/ECS/Components.hpp"
#include "Surge/ECS/Scene.hpp"
#include "Surge/Physics/Physics.hpp"

namespace Surge::ScriptBinding
{
    template<typename T>
    static void BindComponentToEntity(sol::usertype<Entity>& entityType, const String& componentName)
    {
        entityType["Has" + componentName] = &Entity::HasComponent<T>;
        entityType["Add" + componentName] = [](Entity& e) -> T& { return e.AddComponent<T>(); };
        entityType["Remove" + componentName] = &Entity::RemoveComponent<T>;

        // Property getter (e.g. entity.TransformC)
        entityType[componentName] = sol::property([](Entity& e) -> T* {
            if(e.HasComponent<T>())
                return &e.GetComponent<T>();
            return nullptr;
         });
    }

    void BindEntity(void* luaState)
    {
        sol::state_view& lua = *static_cast<sol::state_view*>(luaState);

        auto entityType = lua.new_usertype<Entity>("Entity", sol::no_constructor);
        entityType["IsValid"] = [](const Entity& e) { return static_cast<bool>(e); };
        entityType["Destroy"] = [](Entity& e) {
            if(e)
                e.GetScene()->DestroyEntity(e);
            };

        BindComponentToEntity<NameComponent>(entityType, "NameC");
        BindComponentToEntity<TransformComponent>(entityType, "TransformC");
        BindComponentToEntity<SpriteRendererComponent>(entityType, "SpriteRendererC");
        BindComponentToEntity<CameraComponent>(entityType, "CameraC");
        BindComponentToEntity<MeshComponent>(entityType, "MeshC");
        BindComponentToEntity<LightComponent>(entityType, "LightC");
        BindComponentToEntity<EnvironmentComponent>(entityType, "EnvironmentC");
        BindComponentToEntity<RigidbodyComponent>(entityType, "RigidbodyC");
        BindComponentToEntity<BoxColliderComponent>(entityType, "BoxColliderC");
        BindComponentToEntity<SphereColliderComponent>(entityType, "SphereColliderC");
        BindComponentToEntity<CapsuleColliderComponent>(entityType, "CapsuleColliderC");
        BindComponentToEntity<CylinderColliderComponent>(entityType, "CylinderColliderC");
        BindComponentToEntity<ConvexColliderComponent>(entityType, "ConvexColliderC");
        BindComponentToEntity<MeshColliderComponent>(entityType, "MeshColliderC");
        BindComponentToEntity<ScriptComponent>(entityType, "ScriptC");
    }

    void BindComponents(void* luaState)
    {
        sol::state_view& lua = *static_cast<sol::state_view*>(luaState);

        // ENUMS
        lua.new_enum("RigidbodyType",
                     "STATIC", RigidbodyType::STATIC,
                     "DYNAMIC", RigidbodyType::DYNAMIC,
                     "KINEMATIC", RigidbodyType::KINEMATIC
        );

        lua.new_enum("LightType",
                     "DIRECTIONAL", LightType::DIRECTIONAL,
                     "POINT", LightType::POINT
        );

        // COMPONENTS
        lua.new_usertype<NameComponent>("NameComponent", sol::no_constructor,
                                        "Name", &NameComponent::Name
        );

        lua.new_usertype<TransformComponent>("TransformComponent", sol::no_constructor,
            "Position", sol::property(
                [](TransformComponent& t) -> glm::vec3 { return t.Position; },
                [](TransformComponent& t, const glm::vec3& val) { t.Position = val; t.MarkDirty(); }
            ),
            "Rotation", sol::property(
                [](TransformComponent& t) -> glm::vec3 { return t.Rotation; },
                [](TransformComponent& t, const glm::vec3& val) { t.Rotation = val; t.MarkDirty(); }
            ),
            "Scale", sol::property(
                [](TransformComponent& t) -> glm::vec3 { return t.Scale; },
                [](TransformComponent& t, const glm::vec3& val) { t.Scale = val; t.MarkDirty(); }
            ),
            // AAA Convenience Methods: Instantly applies offsets AND marks dirty!
            "Translate", [](TransformComponent& t, const glm::vec3& offset) { t.Position += offset; t.MarkDirty(); },
            "Rotate", [](TransformComponent& t, const glm::vec3& offset) { t.Rotation += offset; t.MarkDirty(); },
            "ScaleBy", [](TransformComponent& t, const glm::vec3& multi) { t.Scale *= multi; t.MarkDirty(); },
            "MarkDirty", &TransformComponent::MarkDirty,
            "IsDirty", &TransformComponent::IsDirty
        );

        // RENDERER
        lua.new_usertype<SpriteRendererComponent>("SpriteRendererComponent", sol::no_constructor,
                                                  "Color", &SpriteRendererComponent::Color
        );

        lua.new_usertype<CameraComponent>("CameraComponent", sol::no_constructor,
                                          "Primary", &CameraComponent::Primary,
                                          "FixedAspectRatio", &CameraComponent::FixedAspectRatio
        );

        lua.new_usertype<MeshComponent>("MeshComponent", sol::no_constructor,
                                        "MeshID", &MeshComponent::MeshID,
                                        "DropShadow", &MeshComponent::DropShadow
        );

        lua.new_usertype<LightComponent>("LightComponent", sol::no_constructor,
                                         "Color", &LightComponent::Color,
                                         "Intensity", &LightComponent::Intensity,
                                         "Radius", &LightComponent::Radius,
                                         "Falloff", &LightComponent::Falloff,
                                         "Type", &LightComponent::Type
        );

        lua.new_usertype<EnvironmentComponent>("EnvironmentComponent", sol::no_constructor,
                                               "Elevation", &EnvironmentComponent::Elevation,
                                               "Azimuth", &EnvironmentComponent::Azimuth,
                                               "Turbidity", &EnvironmentComponent::Turbidity,
                                               "Exposure", &EnvironmentComponent::Exposure,
                                               "SunIntensity", &EnvironmentComponent::SunIntensity,
                                               "EnableSunDisk", &EnvironmentComponent::EnableSunDisk,
                                               "SkyAmbient", &EnvironmentComponent::SkyAmbient,
                                               "HorizonAmbient", &EnvironmentComponent::HorizonAmbient,
                                               "GroundAmbient", &EnvironmentComponent::GroundAmbient
        );

        // PHYSICS
        lua.new_usertype<RigidbodyComponent>("RigidbodyComponent", sol::no_constructor,
                                             "Type", &RigidbodyComponent::Type,
                                             "Mass", &RigidbodyComponent::Mass,
                                             "UseGravity", &RigidbodyComponent::UseGravity,
                                             "IsSensor", &RigidbodyComponent::IsSensor,
                                             "ContinuousCollision", &RigidbodyComponent::ContinuousCollision,
                                             "FreezeRotationX", &RigidbodyComponent::FreezeRotationX,
                                             "FreezeRotationY", &RigidbodyComponent::FreezeRotationY,
                                             "FreezeRotationZ", &RigidbodyComponent::FreezeRotationZ,
                                             "LinearDamping", &RigidbodyComponent::LinearDamping,
                                             "AngularDamping", &RigidbodyComponent::AngularDamping,
                                             "Friction", &RigidbodyComponent::Friction,
                                             "Bounciness", &RigidbodyComponent::Bounciness,
                                             
                                             "AddForce", [](RigidbodyComponent& rb, const glm::vec3& force) {
                                                 Physics* physics = Core::GetPhysics();
                                                 if(!physics->IsInValid(rb.RuntimeBodyID))
                                                     physics->AddForce(rb.RuntimeBodyID, force);
                                             },
                                             "AddImpulse", [](RigidbodyComponent& rb, const glm::vec3& impulse) {
                                                 Physics* physics = Core::GetPhysics();
                                                 if(!physics->IsInValid(rb.RuntimeBodyID))
                                                     physics->AddImpulse(rb.RuntimeBodyID, impulse);
                                             },
                                             "SetLinearVelocity", [](RigidbodyComponent& rb, const glm::vec3& vel) {
                                                 Physics* physics = Core::GetPhysics();
                                                 if(!physics->IsInValid(rb.RuntimeBodyID))
                                                     physics->SetLinearVelocity(rb.RuntimeBodyID, vel);
                                             },
                                             "GetLinearVelocity", [](RigidbodyComponent& rb) -> glm::vec3 {
                                                 Physics* physics = Core::GetPhysics();
                                                 if(!physics->IsInValid(rb.RuntimeBodyID)) return physics->GetLinearVelocity(rb.RuntimeBodyID);
                                                 return glm::vec3(0.0f);
                                             },
                                             "AddTorque", [](RigidbodyComponent& rb, const glm::vec3& torque) {
                                                 Physics* physics = Core::GetPhysics();
                                                 if(!physics->IsInValid(rb.RuntimeBodyID)) physics->AddTorque(rb.RuntimeBodyID, torque);
                                             },
                                             "AddAngularImpulse", [](RigidbodyComponent& rb, const glm::vec3& impulse) {
                                                 Physics* physics = Core::GetPhysics();
                                                 if(!physics->IsInValid(rb.RuntimeBodyID)) physics->AddAngularImpulse(rb.RuntimeBodyID, impulse);
                                             },
                                             "SetAngularVelocity", [](RigidbodyComponent& rb, const glm::vec3& vel) {
                                                 Physics* physics = Core::GetPhysics();
                                                 if(!physics->IsInValid(rb.RuntimeBodyID))
                                                     physics->SetAngularVelocity(rb.RuntimeBodyID, vel);
                                             },
                                             "GetAngularVelocity", [](RigidbodyComponent& rb) -> glm::vec3 {
                                                 Physics* physics = Core::GetPhysics();
                                                 if(!physics->IsInValid(rb.RuntimeBodyID)) return physics->GetAngularVelocity(rb.RuntimeBodyID);
                                                 return glm::vec3(0.0f);
                                             }
        );

        lua.new_usertype<BoxColliderComponent>("BoxColliderComponent", sol::no_constructor,
                                               "ShowCollider", &BoxColliderComponent::ShowCollider,
                                               "HalfExtents", &BoxColliderComponent::HalfExtents
        );

        lua.new_usertype<SphereColliderComponent>("SphereColliderComponent", sol::no_constructor,
                                                  "ShowCollider", &SphereColliderComponent::ShowCollider,
                                                  "Radius", &SphereColliderComponent::Radius
        );

        lua.new_usertype<CapsuleColliderComponent>("CapsuleColliderComponent", sol::no_constructor,
                                                   "ShowCollider", &CapsuleColliderComponent::ShowCollider,
                                                   "Height", &CapsuleColliderComponent::Height,
                                                   "Radius", &CapsuleColliderComponent::Radius
        );

        lua.new_usertype<CylinderColliderComponent>("CylinderColliderComponent", sol::no_constructor,
                                                    "ShowCollider", &CylinderColliderComponent::ShowCollider,
                                                    "Height", &CylinderColliderComponent::Height,
                                                    "Radius", &CylinderColliderComponent::Radius
        );

        lua.new_usertype<ConvexColliderComponent>("ConvexColliderComponent", sol::no_constructor,
                                                  "LocalOffset", &ConvexColliderComponent::LocalOffset,
                                                  "LocalRotation", &ConvexColliderComponent::LocalRotation,
                                                  "ShowCollider", &ConvexColliderComponent::ShowCollider
        );

        lua.new_usertype<MeshColliderComponent>("MeshColliderComponent", sol::no_constructor,
                                                "LocalOffset", &MeshColliderComponent::LocalOffset,
                                                "LocalRotation", &MeshColliderComponent::LocalRotation
        );

        lua.new_usertype<ScriptComponent>("ScriptComponent", sol::no_constructor,
                                          "ScriptAsset", &ScriptComponent::ScriptAsset
        );
    }
}
