// Copyright (c) - SurgeTechnologies - All rights reserved
#include "ECSBindings.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/ScriptEngine/Lua.hpp"
#include "Surge/ECS/Components.hpp"
#include "Surge/ECS/Scene.hpp"
#include "Surge/Physics/Physics.hpp"
#include "Surge/Audio/AudioEngine.hpp"
#include "BindingUtils.hpp"
#include "Surge/Graphics/HighLevel/Mesh.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "../ScriptAsset.hpp"

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

        auto entityType = lua.new_usertype<Entity>("Entity", sol::no_constructor,
                                                   sol::meta_function::to_string, [](Entity& e) {
                                                       if(!static_cast<bool>(e)) return String("Entity (Invalid/Null)");
                                                       if(e.HasComponent<NameComponent>()) return String("Entity (") + e.GetComponent<NameComponent>().Name + ")";
                                                       return String("Entity (Unnamed)");
                                                   },
                                                   sol::meta_function::equal_to, [](Entity& a, Entity& b) { return a == b; });

        entityType["IsValid"] = [](const Entity& e) { return static_cast<bool>(e); };
        entityType["Destroy"] = [](Entity& e) { if(e) e.GetScene()->DestroyEntity(e); };
        entityType["FindEntityByName"] = [](Entity& e, const String& targetName) -> Entity { return e.GetScene()->GetEntityByName(targetName); };
        entityType["CreateEntity"] = [](Entity& e, const String& name) -> Entity {

            Entity outEntity = {};
            if(Scene* scene = e.GetScene())
                scene->CreateEntity(outEntity, name);

            if (!outEntity)
                Log<Severity::Warn>("ECSBindings: Cannot create entity. Parent entity has no valid Scene reference!");

            return outEntity;
        };
        entityType["SetParent"] = [](Entity& child, Entity& parent) {
            if(child && parent)
                child.GetScene()->SetParent(child, parent);
            else
                Log<Severity::Warn>("ECSBindings: Cannot set parent. Invalid child or parent entity.");
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
        BindComponentToEntity<TextComponent>(entityType, "TextC");
        BindComponentToEntity<UICanvasComponent>(entityType, "UICanvasC");
        BindComponentToEntity<AudioSourceComponent>(entityType, "AudioSourceC");
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

        lua.new_enum("TextAlignment",
                     "LEFT", TextAlignment::LEFT,
                     "CENTER", TextAlignment::CENTER,
                     "RIGHT", TextAlignment::RIGHT
        );

        lua.new_enum("TextVerticalAlignment",
                     "TOP", TextVerticalAlignment::TOP,
                     "CENTER", TextVerticalAlignment::CENTER,
                     "BOTTOM", TextVerticalAlignment::BOTTOM
        );

        lua.new_enum("AttenuationModel",
                     "NONE", AttenuationModel::NONE,
                     "INVERSE_DISTANCE", AttenuationModel::INVERSE_DISTANCE,
                     "LINEAR_DISTANCE", AttenuationModel::LINEAR_DISTANCE,
                     "EXPONENTIAL_DISTANCE", AttenuationModel::EXPONENTIAL_DISTANCE
        );

        // COMPONENTS
        lua.new_usertype<NameComponent>("NameComponent", sol::no_constructor,
                                        "Name", BIND_PROP(NameComponent, Name),
                                        STRICT_READ(NameComponent)
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

                                             "Translate", [](TransformComponent& t, const glm::vec3& offset) { t.Position += offset; t.MarkDirty(); },
                                             "Rotate", [](TransformComponent& t, const glm::vec3& offset) { t.Rotation += offset; t.MarkDirty(); },
                                             "ScaleBy", [](TransformComponent& t, const glm::vec3& multi) { t.Scale *= multi; t.MarkDirty(); },
                                             "MarkDirty", &TransformComponent::MarkDirty,
                                             "IsDirty", &TransformComponent::IsDirty,
                                             STRICT_READ(TransformComponent)
        );

        // RENDERER
        lua.new_usertype<SpriteRendererComponent>("SpriteRendererComponent", sol::no_constructor, 
                                                  "Active", BIND_PROP(SpriteRendererComponent, Active),
                                                  "Color", BIND_PROP(SpriteRendererComponent, Color),
                                                  STRICT_READ(SpriteRendererComponent));

        lua.new_usertype<CameraComponent>("CameraComponent", sol::no_constructor,
                                          "Active", BIND_PROP(CameraComponent, Active),
                                          "Primary", BIND_PROP(CameraComponent, Primary),
                                          "FixedAspectRatio", BIND_PROP(CameraComponent, FixedAspectRatio),
                                          STRICT_READ(CameraComponent));

        lua.new_usertype<MeshComponent>("MeshComponent", sol::no_constructor,
                                        "Active", BIND_PROP(MeshComponent, Active),
                                        "DropShadow", BIND_PROP(MeshComponent, DropShadow),
                                        "SetMesh", [](MeshComponent& mc, const String& meshAssetPath)
                                        {
                                            AssetManager* am = Core::GetAssetManager();
                                            AssetID id = am->GetIDFromPath(meshAssetPath);
                                            if(id)
                                            {
                                                Ref<Mesh> mesh = am->Load<Mesh>(id);
                                                if(mesh)
                                                    mc.MeshAsset = mesh;
                                                else
                                                    Log<Severity::Warn>("[ECSBindings.cpp] Lua: MeshComponent: Failed to load mesh at path {}", meshAssetPath);
                                            }
                                            else
                                                Log<Severity::Warn>("[ECSBindings.cpp] Lua: MeshComponent: Failed to load mesh at path {}", meshAssetPath);
                                        },
                                        STRICT_READ(MeshComponent));

        lua.new_usertype<LightComponent>("LightComponent", sol::no_constructor,
                                         "Active", BIND_PROP(LightComponent, Active),
                                         "Color", BIND_PROP(LightComponent, Color),
                                         "Intensity", BIND_PROP(LightComponent, Intensity),
                                         "Radius", BIND_PROP(LightComponent, Radius),
                                         "Falloff", BIND_PROP(LightComponent, Falloff),
                                         "Type", BIND_PROP(LightComponent, Type),
                                         STRICT_READ(LightComponent));

        lua.new_usertype<EnvironmentComponent>("EnvironmentComponent", sol::no_constructor,
                                               "Active", BIND_PROP(EnvironmentComponent, Active),
                                               "Elevation", BIND_PROP(EnvironmentComponent, Elevation),
                                               "Azimuth", BIND_PROP(EnvironmentComponent, Azimuth),
                                               "Turbidity", BIND_PROP(EnvironmentComponent, Turbidity),
                                               "Exposure", BIND_PROP(EnvironmentComponent, Exposure),
                                               "SunIntensity", BIND_PROP(EnvironmentComponent, SunIntensity),
                                               "EnableSunDisk", BIND_PROP(EnvironmentComponent, EnableSunDisk),
                                               "SkyAmbient", BIND_PROP(EnvironmentComponent, SkyAmbient),
                                               "HorizonAmbient", BIND_PROP(EnvironmentComponent, HorizonAmbient),
                                               "GroundAmbient", BIND_PROP(EnvironmentComponent, GroundAmbient),
                                               STRICT_READ(EnvironmentComponent));

        // PHYSICS
        lua.new_usertype<RigidbodyComponent>("RigidbodyComponent", sol::no_constructor,
                                             "Active", BIND_PROP(RigidbodyComponent, Active),
                                             "Type", BIND_PROP(RigidbodyComponent, Type),
                                             "Mass", BIND_PROP(RigidbodyComponent, Mass),
                                             "IsSensor", BIND_PROP(RigidbodyComponent, IsSensor),
                                             "ContinuousCollision", BIND_PROP(RigidbodyComponent, ContinuousCollision),
                                             "FreezeRotationX", BIND_PROP(RigidbodyComponent, FreezeRotationX),
                                             "FreezeRotationY", BIND_PROP(RigidbodyComponent, FreezeRotationY),
                                             "FreezeRotationZ", BIND_PROP(RigidbodyComponent, FreezeRotationZ),
                                             "LinearDamping", BIND_PROP(RigidbodyComponent, LinearDamping),
                                             "AngularDamping", BIND_PROP(RigidbodyComponent, AngularDamping),
                                             "Friction", BIND_PROP(RigidbodyComponent, Friction),
                                             "Bounciness", BIND_PROP(RigidbodyComponent, Bounciness),

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
                                             },
                                             STRICT_READ(RigidbodyComponent));

        lua.new_usertype<BoxColliderComponent>("BoxColliderComponent", sol::no_constructor,
                                               "Active", BIND_PROP(BoxColliderComponent, Active),
                                               "ShowCollider", BIND_PROP(BoxColliderComponent, ShowCollider),
                                               "HalfExtents", BIND_PROP(BoxColliderComponent, HalfExtents),
                                               STRICT_READ(BoxColliderComponent));

        lua.new_usertype<SphereColliderComponent>("SphereColliderComponent", sol::no_constructor,
                                                  "Active", BIND_PROP(SphereColliderComponent, Active),
                                                  "ShowCollider", BIND_PROP(SphereColliderComponent, ShowCollider),
                                                  "Radius", BIND_PROP(SphereColliderComponent, Radius),
                                                  STRICT_READ(SphereColliderComponent));

        lua.new_usertype<CapsuleColliderComponent>("CapsuleColliderComponent", sol::no_constructor,
                                                   "Active", BIND_PROP(CapsuleColliderComponent, Active),
                                                   "ShowCollider", BIND_PROP(CapsuleColliderComponent, ShowCollider),
                                                   "Height", BIND_PROP(CapsuleColliderComponent, Height),
                                                   "Radius", BIND_PROP(CapsuleColliderComponent, Radius),
                                                   STRICT_READ(CapsuleColliderComponent));

        lua.new_usertype<CylinderColliderComponent>("CylinderColliderComponent", sol::no_constructor,
                                                    "Active", BIND_PROP(CylinderColliderComponent, Active),
                                                    "ShowCollider", BIND_PROP(CylinderColliderComponent, ShowCollider),
                                                    "Height", BIND_PROP(CylinderColliderComponent, Height),
                                                    "Radius", BIND_PROP(CylinderColliderComponent, Radius),
                                                    STRICT_READ(CylinderColliderComponent));

        lua.new_usertype<ConvexColliderComponent>("ConvexColliderComponent", sol::no_constructor,
                                                  "Active", BIND_PROP(ConvexColliderComponent, Active),
                                                  "ShowCollider", BIND_PROP(ConvexColliderComponent, ShowCollider),
                                                  "LocalOffset", sol::property(
                                                      [](ConvexColliderComponent& c) -> glm::vec3 { return c.LocalOffset; },
                                                      [](ConvexColliderComponent& c, const glm::vec3& v) { c.LocalOffset = v; c.IsDirty = true; }
                                                  ),
                                                  "LocalRotation", sol::property(
                                                      [](ConvexColliderComponent& c) -> glm::vec3 { return c.LocalRotation; },
                                                      [](ConvexColliderComponent& c, const glm::vec3& v) { c.LocalRotation = v; c.IsDirty = true; }),

                                                  STRICT_READ(ConvexColliderComponent));

        lua.new_usertype<MeshColliderComponent>("MeshColliderComponent", sol::no_constructor,
                                                "Active", BIND_PROP(MeshColliderComponent, Active),
                                                "LocalOffset", BIND_PROP(MeshColliderComponent, LocalOffset),
                                                "LocalRotation", BIND_PROP(MeshColliderComponent, LocalRotation),
                                                STRICT_READ(MeshColliderComponent));

        lua.new_usertype<ScriptComponent>("ScriptComponent", sol::no_constructor,
                                          "Active", BIND_PROP(ScriptComponent, Active),
                                          "ScriptAsset", BIND_PROP(ScriptComponent, ScriptAsset),
                                          "SetScript", [](ScriptComponent& sc, const String& scriptAssetPath)
                                          {
                                              AssetManager* am = Core::GetAssetManager();
                                              AssetID id = am->GetIDFromPath(scriptAssetPath);
                                              if(id)
                                              {
                                                  Ref<Script> script = am->Load<Script>(id);
                                                  if(script)
                                                      sc.ScriptAsset = script;
                                                  else
                                                      Log<Severity::Warn>("[ECSBindings.cpp] Lua: ScriptComponent: Failed to load script at path {}", scriptAssetPath);
                                              }
                                              else
                                                  Log<Severity::Warn>("[ECSBindings.cpp] Lua: ScriptComponent: Failed to load script at path {}", scriptAssetPath);
                                          },
                                          STRICT_READ(ScriptComponent));

        lua.new_usertype<TextComponent>("TextComponent", sol::no_constructor,
                                        "Active", BIND_PROP(TextComponent, Active),
                                        "Text", BIND_PROP(TextComponent, Text),
                                        "Color", BIND_PROP(TextComponent, Color),
                                        "MaxWidth", BIND_PROP(TextComponent, MaxWidth),
                                        "LetterSpacing", BIND_PROP(TextComponent, LetterSpacing),
                                        "Alignment", BIND_PROP(TextComponent, Alignment),
                                        "VerticalAlignment", BIND_PROP(TextComponent, VerticalAlignment),
                                        "LineSpacing", BIND_PROP(TextComponent, LineSpacing),
                                        "ShadowEnabled", BIND_PROP(TextComponent, ShadowEnabled),
                                        "ShadowColor", BIND_PROP(TextComponent, ShadowColor),
                                        "ShadowOffset", BIND_PROP(TextComponent, ShadowOffset),
                                        STRICT_READ(TextComponent));

        lua.new_usertype<UICanvasComponent>("UICanvasComponent", sol::no_constructor,
                                        "Active", BIND_PROP(UICanvasComponent, Active),
                                        "ShowCanvas", BIND_PROP(UICanvasComponent, ShowCanvas),
                                        "ScriptAsset", BIND_PROP(UICanvasComponent, ScriptAsset),
                                        STRICT_READ(UICanvasComponent));

        // AUDIO
        lua.new_usertype<AudioSourceComponent>("AudioSourceComponent", sol::no_constructor,
                                        "Active", BIND_PROP(AudioSourceComponent, Active),
                                        "PlayOnAwake", BIND_PROP(AudioSourceComponent, PlayOnAwake),
                                        "IsSpatialized", BIND_PROP(AudioSourceComponent, IsSpatialized),
                                        "IsStreaming", BIND_PROP(AudioSourceComponent, IsStreaming),
                                        "IsPlaying", sol::property([](AudioSourceComponent& c) { return c.IsPlaying; }),
                                        "IsInitialized", sol::property([](AudioSourceComponent& c) { return c.IsInitialized; }),
                                        "Volume", sol::property(
                                            [](AudioSourceComponent& c) { return c.Volume; },
                                            [](AudioSourceComponent& c, float v) {
                                                c.Volume = v;
                                                if(c.RuntimeID) Core::GetAudioEngine()->SetVolume(c.RuntimeID, v);
                                            }),
                                        "Pitch", sol::property(
                                            [](AudioSourceComponent& c) { return c.Pitch; },
                                            [](AudioSourceComponent& c, float v) {
                                                c.Pitch = v;
                                                if(c.RuntimeID) Core::GetAudioEngine()->SetPitch(c.RuntimeID, v);
                                            }),

                                        "Loop", sol::property(
                                            [](AudioSourceComponent& c) { return c.Loop; },
                                            [](AudioSourceComponent& c, bool v) {
                                                c.Loop = v;
                                                if(c.RuntimeID) Core::GetAudioEngine()->SetLooping(c.RuntimeID, v);
                                            }),
                                        "Attenuation", sol::property(
                                            [](AudioSourceComponent& c) { return c.Attenuation; },
                                            [](AudioSourceComponent& c, AttenuationModel a) {
                                                c.Attenuation = a;
                                                if(c.RuntimeID) Core::GetAudioEngine()->SetAttenuationModel(c.RuntimeID, a);
                                            }),
                                        "MinDistance", sol::property(
                                            [](AudioSourceComponent& c) { return c.MinDistance; },
                                            [](AudioSourceComponent& c, float v) {
                                                c.MinDistance = v;
                                                if(c.RuntimeID) Core::GetAudioEngine()->SetMinDistance(c.RuntimeID, v);
                                            }),
                                        "MaxDistance", sol::property(
                                            [](AudioSourceComponent& c) { return c.MaxDistance; },
                                            [](AudioSourceComponent& c, float v) {
                                                c.MaxDistance = v;
                                                if(c.RuntimeID) Core::GetAudioEngine()->SetMaxDistance(c.RuntimeID, v);
                                            }),
                                        "Play", [](AudioSourceComponent& ac) {
                                            if(ac.RuntimeID)
                                            {
                                                Core::GetAudioEngine()->PlayAudio(ac.RuntimeID);
                                                ac.IsPlaying = true;
                                            }
                                            else
                                                Log<Severity::Warn>("ECSBindings: AudioSourceComponent is not initialized. Cannot play audio!");
                                        },
                                        "Stop", [](AudioSourceComponent& ac) {
                                            if(ac.RuntimeID)
                                            {
                                                Core::GetAudioEngine()->StopAudio(ac.RuntimeID);
                                                ac.IsPlaying = false;
                                            }
                                            else
                                                Log<Severity::Warn>("ECSBindings: AudioSourceComponent is not initialized. Cannot stop audio!");
                                        }
        );
    }
}
