// Copyright (c) - SurgeTechnologies - All rights reserved
#include "ECSBindings.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/ScriptEngine/Lua.hpp"
#include "Surge/ECS/Components/Components.hpp"
#include "Surge/ECS/Components/ScriptComponent.hpp"
#include "Surge/ECS/Scene.hpp"
#include "Surge/Physics/Physics.hpp"
#include "Surge/Audio/AudioEngine.hpp"
#include "BindingUtils.hpp"
#include "Surge/Graphics/HighLevel/Mesh.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/ScriptEngine/ScriptAsset.hpp"
#include "Surge/ScriptEngine/ScriptEngine.hpp"
#include "Surge/Audio/Audio.hpp"
#include <glm/glm.hpp>
// TODO: REMOVE following includes
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Utility/Platform.hpp"



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
            if(e && e.HasComponent<T>())
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
        entityType["DestroyImmediate"] = [](Entity& e) { if(e) e.GetScene()->DestroyEntityImmediate(e); };
        entityType["FindEntityByName"] = [](Entity& e, const String& targetName) -> Entity { return e.GetScene()->GetEntityByName(targetName); };
        entityType["CreateEntity"] = [](Entity& e, const String& name) -> Entity {

            Entity outEntity = {};
            if(Scene* scene = e.GetScene())
                scene->CreateEntity(outEntity, name);

            if (!outEntity)
                Log<Severity::Warn>("ECSBindings: Cannot create entity. Parent entity has no valid Scene reference!");

            return outEntity;
        };
        // The environment of one of the entity's scripts: its globals, functions and Properties. With a name ("Player", the script's
        // file name) that script, without one the first script (or the UI canvas script). nil until the script was instantiated;
        // every script starting in a frame is instantiated before any OnCreate runs
        entityType["GetScript"] = [](Entity& e, sol::optional<String> name) -> sol::object {
            if(!e)
                return sol::object();
            if(const ScriptComponent* scripts = e.TryGetComponent<ScriptComponent>())
            {
                for(const ScriptEntry& entry : scripts->Scripts)
                {
                    if(entry.Instance.IsValid() && (!name || ScriptEngine::GetScriptName(entry.ScriptAsset) == *name))
                        return entry.Instance.Env;
                }
            }
            if(const UICanvasComponent* canvas = e.TryGetComponent<UICanvasComponent>(); canvas && canvas->Instance.IsValid())
            {
                if(!name || ScriptEngine::GetScriptName(canvas->ScriptAsset) == *name)
                    return canvas->Instance.Env;
            }
            return sol::object();
        };
        // Every instantiated script of the entity, in order
        entityType["GetScripts"] = [](Entity& e, sol::this_state s) -> sol::table {
            sol::state_view lua(s);
            sol::table result = lua.create_table();
            if(const ScriptComponent* scripts = e ? e.TryGetComponent<ScriptComponent>() : nullptr)
            {
                int index = 1;
                for(const ScriptEntry& entry : scripts->Scripts)
                {
                    if(entry.Instance.IsValid())
                        result[index++] = entry.Instance.Env;
                }
            }
            return result;
        };
        entityType["SetParent"] = [](Entity& child, Entity& parent) {
            if(child && parent)
                child.GetScene()->SetParent(child, parent);
            else
                Log<Severity::Warn>("ECSBindings: Cannot set parent. Invalid child or parent entity.");
        };

        //BindComponentToEntity<IDComponent>(entityType, "IDC");
        BindComponentToEntity<NameComponent>(entityType, "NameC");
        BindComponentToEntity<TransformComponent>(entityType, "TransformC");
        BindComponentToEntity<SpriteRendererComponent>(entityType, "SpriteRendererC");
        BindComponentToEntity<CameraComponent>(entityType, "CameraC");
        BindComponentToEntity<MeshComponent>(entityType, "MeshC");
        BindComponentToEntity<LightComponent>(entityType, "LightC");
        BindComponentToEntity<EnvironmentComponent>(entityType, "EnvironmentC");
        BindComponentToEntity<RendererSettingsComponent>(entityType, "RendererSettingsC");
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
        BindComponentToEntity<ParticleSystemComponent>(entityType, "ParticleSystemC");
        BindComponentToEntity<AnimatorComponent>(entityType, "AnimatorC");
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

        lua.new_enum("RenderProfile",
                     "DESKTOP", RenderProfile::DESKTOP,
                     "MOBILE", RenderProfile::MOBILE
        );

        lua.new_enum("ShadowQuality",
                     "LOW", ShadowQuality::LOW,
                     "MEDIUM", ShadowQuality::MEDIUM,
                     "HIGH", ShadowQuality::HIGH,
                     "ULTRA", ShadowQuality::ULTRA
        );

        lua.new_enum("AOQuality",
                     "LOW", AOQuality::LOW,
                     "MEDIUM", AOQuality::MEDIUM,
                     "HIGH", AOQuality::HIGH,
                     "ULTRA", AOQuality::ULTRA
        );

        lua.new_enum("BloomQuality",
                     "LOW", BloomQuality::LOW,
                     "MEDIUM", BloomQuality::MEDIUM,
                     "HIGH", BloomQuality::HIGH,
                     "ULTRA", BloomQuality::ULTRA
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
        // Fkass lua doesnt have any concept for unsigned 64 bit int
        //lua.new_usertype<IDComponent>("IDComponent", sol::no_constructor,
        //                              "Get", [](IDComponent& id) -> uint64_t { return id.ID.Get(); },
        //                               STRICT_READ(IDComponent)
        //);

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

        // Material Asset
        auto materialType = lua.new_usertype<Material>("Material", sol::no_constructor,
                                                       "MarkDirty", &Material::MarkDirty,

                                                       // TODO: Texture Binding
                                                       "SetFloat", [](Material& m, const String& name, float val) { m.Set<float>(name, val); },
                                                       "GetFloat", [](Material& m, const String& name) -> float { return m.Get<float>(name); },
                                                       "SetInt", [](Material& m, const String& name, int val) { m.Set<int>(name, val); },
                                                       "GetInt", [](Material& m, const String& name) -> int { return m.Get<int>(name); },
                                                       "SetVec2", [](Material& m, const String& name, const glm::vec2& val) { m.Set<glm::vec2>(name, val); },
                                                       "GetVec2", [](Material& m, const String& name) -> glm::vec2 { return m.Get<glm::vec2>(name); },
                                                       "SetVec3", [](Material& m, const String& name, const glm::vec3& val) { m.Set<glm::vec3>(name, val); },
                                                       "GetVec3", [](Material& m, const String& name) -> glm::vec3 { return m.Get<glm::vec3>(name); },
                                                       "SetVec4", [](Material& m, const String& name, const glm::vec4& val) { m.Set<glm::vec4>(name, val); },
                                                       "GetVec4", [](Material& m, const String& name) -> glm::vec4 { return m.Get<glm::vec4>(name); }
        );
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
                                        // A Ref: scripts may keep the material after its mesh is unloaded (AssetManager::UnloadUnreferenced on scene change)
                                        "GetMaterial", [](MeshComponent& mc, Uint index) -> Ref<Material> {
                                            if(mc.MeshAsset)
                                            {
                                                Ref<Mesh> mesh = mc.MeshAsset.As<Mesh>();
                                                if(mesh)
                                                    return mesh->GetMaterialAtIndex(index);
                                            }
                                            Log<Severity::Warn>("[ECSBindings.cpp] Lua: Tried to get material on a MeshComponent that has no valid MeshAsset!");
                                            return nullptr;
                                        },

                                        "SetMaterialOverride", sol::overload(
                                            // Overload 1: Load from a file path string
                                            [](MeshComponent& mc, Uint index, const String& materialPath) {
                                                if(mc.MeshAsset)
                                                {
                                                    Ref<Mesh> mesh = mc.MeshAsset.As<Mesh>();
                                                    if(mesh)
                                                    {
                                                        AssetManager* am = Core::GetAssetManager();
                                                        AssetID id = am->GetIDFromPath(materialPath);
                                                        if(id)
                                                        {
                                                            Ref<Material> mat = am->Load<Material>(id);
                                                            if(mat)
                                                            {
                                                                mesh->SetMaterialOverride(index, mat);
                                                                return;
                                                            }
                                                        }
                                                        Log<Severity::Warn>("[ECSBindings.cpp] Lua: Failed to load material override at path {}", materialPath);
                                                    }
                                                }
                                            },
                                            // Overload 2: Pass an existing Material pointer directly
                                            [](MeshComponent& mc, Uint index, Material* mat) {
                                                if(mc.MeshAsset && mat)
                                                {
                                                    Ref<Mesh> mesh = mc.MeshAsset.As<Mesh>();
                                                    if(mesh)
                                                        mesh->SetMaterialOverride(index, Ref<Material>(mat));
                                                }
                                            }
                                        ),
                                        "ClearMaterialOverride", [](MeshComponent& mc, Uint index) {
                                            if(mc.MeshAsset)
                                            {
                                                Ref<Mesh> mesh = mc.MeshAsset.As<Mesh>();
                                                if(mesh)
                                                    mesh->ClearMaterialOverride(index);
                                            }
                                        },
                                        "HasOverride", [](MeshComponent& mc, Uint index) -> bool {
                                            if(mc.MeshAsset)
                                            {
                                                Ref<Mesh> mesh = mc.MeshAsset.As<Mesh>();
                                                if(mesh)
                                                    return mesh->HasOverride(index);
                                            }
                                            return false;
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

        // RENDER SETTINGS
        // Nested settings are handed out by pointer, so entity.RendererSettingsC.Desktop.Bloom.Intensity = 2 writes into the component.
        // Quality is read only: ApplyQualityPreset() sets it together with the knobs it drives
        lua.new_usertype<ShadowSettings>("ShadowSettings", sol::no_constructor,
                                         "Enable", BIND_PROP(ShadowSettings, Enable),
                                         "Quality", sol::readonly_property([](ShadowSettings& s) { return s.Quality; }),
                                         "ApplyQualityPreset", &ShadowSettings::ApplyQualityPreset,
                                         "CascadeCount", BIND_PROP(ShadowSettings, CascadeCount),
                                         "Resolution", BIND_PROP(ShadowSettings, Resolution),
                                         "FilterSize", BIND_PROP(ShadowSettings, FilterSize),
                                         "DitheredBlend", BIND_PROP(ShadowSettings, DitheredBlend),
                                         "Distance", BIND_PROP(ShadowSettings, Distance),
                                         "FadeRange", BIND_PROP(ShadowSettings, FadeRange),
                                         "CascadeSplitLambda", BIND_PROP(ShadowSettings, CascadeSplitLambda),
                                         "CascadeBlend", BIND_PROP(ShadowSettings, CascadeBlend),
                                         "ShowCascades", BIND_PROP(ShadowSettings, ShowCascades),
                                         "NormalBias", BIND_PROP(ShadowSettings, NormalBias),
                                         "DepthBiasConstant", BIND_PROP(ShadowSettings, DepthBiasConstant),
                                         "DepthBiasClamp", BIND_PROP(ShadowSettings, DepthBiasClamp),
                                         "DepthBiasSlope", BIND_PROP(ShadowSettings, DepthBiasSlope),
                                         STRICT_READ(ShadowSettings));

        lua.new_usertype<AmbientOcclusionSettings>("AmbientOcclusionSettings", sol::no_constructor,
                                                   "Enable", BIND_PROP(AmbientOcclusionSettings, Enable),
                                                   "Quality", sol::readonly_property([](AmbientOcclusionSettings& s) { return s.Quality; }),
                                                   "ApplyQualityPreset", &AmbientOcclusionSettings::ApplyQualityPreset,
                                                   "ResolutionScale", BIND_PROP(AmbientOcclusionSettings, ResolutionScale),
                                                   "SliceCount", BIND_PROP(AmbientOcclusionSettings, SliceCount),
                                                   "StepsPerSlice", BIND_PROP(AmbientOcclusionSettings, StepsPerSlice),
                                                   "BlurRadius", BIND_PROP(AmbientOcclusionSettings, BlurRadius),
                                                   "AccurateNormals", BIND_PROP(AmbientOcclusionSettings, AccurateNormals),
                                                   "Radius", BIND_PROP(AmbientOcclusionSettings, Radius),
                                                   "Intensity", BIND_PROP(AmbientOcclusionSettings, Intensity),
                                                   "FalloffRange", BIND_PROP(AmbientOcclusionSettings, FalloffRange),
                                                   "MaxDistance", BIND_PROP(AmbientOcclusionSettings, MaxDistance),
                                                   "BlurSharpness", BIND_PROP(AmbientOcclusionSettings, BlurSharpness),
                                                   STRICT_READ(AmbientOcclusionSettings));

        lua.new_usertype<BloomSettings>("BloomSettings", sol::no_constructor,
                                        "Enable", BIND_PROP(BloomSettings, Enable),
                                        "Quality", sol::readonly_property([](BloomSettings& s) { return s.Quality; }),
                                        "ApplyQualityPreset", &BloomSettings::ApplyQualityPreset,
                                        "UseCompute", BIND_PROP(BloomSettings, UseCompute),
                                        "MipCount", BIND_PROP(BloomSettings, MipCount),
                                        "HighQualityFilter", BIND_PROP(BloomSettings, HighQualityFilter),
                                        "AntiFlicker", BIND_PROP(BloomSettings, AntiFlicker),
                                        "HighQualityComposite", BIND_PROP(BloomSettings, HighQualityComposite),
                                        "Threshold", BIND_PROP(BloomSettings, Threshold),
                                        "Knee", BIND_PROP(BloomSettings, Knee),
                                        "Intensity", BIND_PROP(BloomSettings, Intensity),
                                        "Scatter", BIND_PROP(BloomSettings, Scatter),
                                        "ClampValue", BIND_PROP(BloomSettings, ClampValue),
                                        "Tint", BIND_PROP(BloomSettings, Tint),
                                        STRICT_READ(BloomSettings));

        lua.new_usertype<PostProcessSettings>("PostProcessSettings", sol::no_constructor,
                                              "VignetteIntensity", BIND_PROP(PostProcessSettings, VignetteIntensity),
                                              "VignetteSoftness", BIND_PROP(PostProcessSettings, VignetteSoftness),
                                              "Grain", BIND_PROP(PostProcessSettings, Grain),
                                              "ShowAOOnly", BIND_PROP(PostProcessSettings, ShowAOOnly),
                                              STRICT_READ(PostProcessSettings));

        lua.new_usertype<RenderSettings>("RenderSettings", sol::no_constructor,
                                         "RenderScale", BIND_PROP(RenderSettings, RenderScale),
                                         "MaxLights", sol::property([](RenderSettings& s) { return s.Lighting.MaxLights; }, [](RenderSettings& s, Uint v) { s.Lighting.MaxLights = v; }),
                                         "MaxParticles", sol::property([](RenderSettings& s) { return s.Particles.MaxParticles; }, [](RenderSettings& s, Uint v) { s.Particles.MaxParticles = v; }),
                                         "EmissionQuality", sol::property([](RenderSettings& s) { return s.Particles.EmissionQuality; }, [](RenderSettings& s, float v) { s.Particles.EmissionQuality = v; }),
                                         "MaxSkinMatrices", sol::property([](RenderSettings& s) { return s.Animation.MaxSkinMatrices; }, [](RenderSettings& s, Uint v) { s.Animation.MaxSkinMatrices = v; }),
                                         "Shadows", sol::readonly_property([](RenderSettings& s) -> ShadowSettings* { return &s.Shadows; }),
                                         "AO", sol::readonly_property([](RenderSettings& s) -> AmbientOcclusionSettings* { return &s.AO; }),
                                         "Bloom", sol::readonly_property([](RenderSettings& s) -> BloomSettings* { return &s.Bloom; }),
                                         "PostProcess", sol::readonly_property([](RenderSettings& s) -> PostProcessSettings* { return &s.PostProcess; }),
                                         STRICT_READ(RenderSettings));

        lua.new_usertype<RendererSettingsComponent>("RendererSettingsComponent", sol::no_constructor,
                                                    "Active", BIND_PROP(RendererSettingsComponent, Active),
                                                    "Desktop", sol::readonly_property([](RendererSettingsComponent& c) -> RenderSettings* { return &c.Desktop; }),
                                                    "Mobile", sol::readonly_property([](RendererSettingsComponent& c) -> RenderSettings* { return &c.Mobile; }),
                                                    // The profile this platform renders with (in the Editor: the previewed one)
                                                    "GetCurrentProfile", [](RendererSettingsComponent& c) -> RenderSettings* { return &c.GetProfileSettings(Core::GetRenderer()->GetRenderProfile()); },
                                                    "ResetProfile", [](RendererSettingsComponent& c, RenderProfile profile) { c.GetProfileSettings(profile) = RenderSettings::Default(profile); },
                                                    STRICT_READ(RendererSettingsComponent));

        // PHYSICS
        lua.new_usertype<RigidbodyComponent>("RigidbodyComponent", sol::no_constructor,
                                             "Active", BIND_PROP(RigidbodyComponent, Active),
                                             "Type", BIND_PROP(RigidbodyComponent, Type),
                                             "Layer", sol::property(
                                                 [](const RigidbodyComponent& rb) { return rb.Layer; },
                                                 [](RigidbodyComponent& rb, int layer) {
                                                     rb.Layer = PhysicsSettings::ClampLayer(layer);
                                                     Physics* physics = Core::GetPhysics();
                                                     if(!physics->IsInValid(rb.RuntimeBodyID))
                                                         physics->SetLayer(rb.RuntimeBodyID, rb.Layer);
                                                 }),
                                             "Mass", BIND_PROP(RigidbodyComponent, Mass),
                                             "Interpolate", BIND_PROP(RigidbodyComponent, Interpolate),
                                             "UseGravity", BIND_PROP(RigidbodyComponent, UseGravity),
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

        // Adds a script asset to the entity. It is instantiated at the start of the next frame (OnCreate, OnEnable)
        auto addScript = [](ScriptComponent& sc, const String& scriptAssetPath) -> bool {
            AssetManager* am = Core::GetAssetManager();
            const AssetID id = am->GetIDFromPath(scriptAssetPath);
            Ref<Script> script = id ? am->Load<Script>(id) : nullptr;
            if(!script)
            {
                Log<Severity::Warn>("[ECSBindings.cpp] Lua: ScriptComponent:AddScript: no script asset at path {}", scriptAssetPath);
                return false;
            }
            sc.PendingScripts.push_back(script);
            return true;
        };
        auto findEntry = [](ScriptComponent& sc, const String& name) -> ScriptEntry* {
            for(ScriptEntry& entry : sc.Scripts)
            {
                if(ScriptEngine::GetScriptName(entry.ScriptAsset) == name)
                    return &entry;
            }
            return nullptr;
        };

        lua.new_usertype<ScriptComponent>("ScriptComponent", sol::no_constructor,
                                          "Active", BIND_PROP(ScriptComponent, Active),
                                          "Count", sol::readonly_property([](ScriptComponent& sc) { return static_cast<lua_Integer>(sc.Scripts.size()); }),
                                          "AddScript", addScript,
                                          "SetScript", addScript, // Kept for older scripts: there is no single script to replace anymore
                                          "HasScript", [findEntry](ScriptComponent& sc, const String& name) { return findEntry(sc, name) != nullptr; },
                                          "IsScriptActive", [findEntry](ScriptComponent& sc, const String& name) {
                                              const ScriptEntry* entry = findEntry(sc, name);
                                              return entry && entry->Active;
                                          },
                                          // OnDisable / OnEnable run when the scene loop reaches that script: this frame if it comes after the caller, else the next
                                          "SetScriptActive", [findEntry](ScriptComponent& sc, const String& name, bool active) {
                                              ScriptEntry* entry = findEntry(sc, name);
                                              if(entry)
                                                  entry->Active = active;
                                              return entry != nullptr;
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
                                        "SortOrder", BIND_PROP(UICanvasComponent, SortOrder),
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
                                        "Seek", [](AudioSourceComponent& ac, float seconds) {
                                            if(ac.RuntimeID)
                                                Core::GetAudioEngine()->SeekTo(ac.RuntimeID, seconds);
                                            else
                                                Log<Severity::Warn>("ECSBindings.cpp: [AudioSourceComponent] AudioSourceComponent is not initialized. Cannot seek!");
                                        },
                                        "Stop", [](AudioSourceComponent& ac) {
                                            if(ac.RuntimeID)
                                            {
                                                Core::GetAudioEngine()->StopAudio(ac.RuntimeID);
                                                ac.IsPlaying = false;
                                            }
                                            else
                                                Log<Severity::Warn>("ECSBindings: AudioSourceComponent is not initialized. Cannot stop audio!");
                                        },
                                        "SetAudioClip", [](AudioSourceComponent& ac, const String& audioAssetPath) {
                                            AssetManager* am = Core::GetAssetManager();
                                            AssetID id = am->GetIDFromPath(audioAssetPath);
                                            if(id)
                                            {
                                                Ref<Audio> clip = am->Load<Audio>(id);
                                                if(clip)
                                                    ac.AudioClip = clip;
                                                else
                                                    Log<Severity::Warn>("[ECSBindings.cpp] Lua: AudioSourceComponent: Failed to load audio at path {}", audioAssetPath);
                                            }
                                            else
                                                Log<Severity::Warn>("[ECSBindings.cpp] Lua: AudioSourceComponent: Failed to load audio at path {}", audioAssetPath);
                                        },
                                        STRICT_READ(AudioSourceComponent)
        );

        //TODO: MOVE to Renderer Binding
        sol::table renderer = lua["Renderer"].get_or_create<sol::table>();
        renderer.set_function("DrawLine", [](const glm::vec3& point0, const glm::vec3& point1, const glm::vec4& color) { Core::GetRenderer()->SubmitLine(point0, point1, color); });
        renderer.set_function("DrawQuad", [](const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scale, const glm::vec4& color)
                              {
                                  glm::mat4 transform = glm::translate(glm::mat4(1.0f), pos) * glm::mat4_cast(glm::quat(glm::radians(rot))) * glm::scale(glm::mat4(1.0f), scale);
                                  Core::GetRenderer()->SubmitQuad(transform, color);
                              });

        // TODO: MOVE to Application Binding
        sol::table application = lua["Application"].get_or_create<sol::table>();
        application.set_function("RequestExit", []() { return Platform::RequestExit(); });
    }
}
