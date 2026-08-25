// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/ECS/Scene.hpp"
#include "Surge/ECS/Components.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Core/Profiler.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Graphics/HighLevel/Mesh.hpp"
#include "Surge/Graphics/HighLevel/DefaultMeshes.hpp"
#include "Surge/Physics/Physics.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/ScriptEngine/ScriptAsset.hpp"

#include "Jolt/Physics/Body/BodyManager.h"
#include "Jolt/Physics/PhysicsSystem.h"
#include <regex>
#include "../Audio/AudioEngine.hpp"
#include "../Audio/Audio.hpp"

namespace Surge
{
    static Entity sSelectedEntity;

    Scene::Scene()
    {
        AddStartupEntities();

        // Are these good here?
        mRegistry.on_destroy<ScriptComponent>().connect<&Scene::OnScriptDestroyed>(this);
        mRegistry.on_destroy<UICanvasComponent>().connect<&Scene::OnUICanvasDestroyed>(this);
        mRegistry.on_destroy<AudioSourceComponent>().connect<&Scene::OnAudioSourceComponentDestroyed>(this);
    }

    Scene::~Scene()
    {
        mRegistry.clear();
        sSelectedEntity = Entity(entt::null, nullptr);
    }

    void Scene::OnRuntimeStart()
    {
        mIsRunning = true;

        // Refresh the asset references from the asset manager to ensure it's loaded and valid
        for(const auto& [entity, script] : mRegistry.view<ScriptComponent>().each())
        {
            if (script.ScriptAsset)
                script.ScriptAsset = Core::GetAssetManager()->Load<Script>(script.ScriptAsset->GetID());
        }
        for(const auto& [entity, canvas] : mRegistry.view<UICanvasComponent>().each())
        {
            if (canvas.ScriptAsset)
                canvas.ScriptAsset = Core::GetAssetManager()->Load<Script>(canvas.ScriptAsset->GetID());
        }

        for(const auto& [entity, rigidbody] : mRegistry.view<RigidbodyComponent>().each())
            OnColliderAdded(mRegistry, entity);

        mRegistry.on_construct<RigidbodyComponent>().connect<&Scene::OnColliderAdded>(this);
        mRegistry.on_construct<BoxColliderComponent>().connect<&Scene::OnColliderAdded>(this);
        mRegistry.on_construct<SphereColliderComponent>().connect<&Scene::OnColliderAdded>(this);
        mRegistry.on_construct<CapsuleColliderComponent>().connect<&Scene::OnColliderAdded>(this);
        mRegistry.on_construct<CylinderColliderComponent>().connect<&Scene::OnColliderAdded>(this);
        mRegistry.on_construct<ConvexColliderComponent>().connect<&Scene::OnColliderAdded>(this);
        mRegistry.on_construct<MeshColliderComponent>().connect<&Scene::OnColliderAdded>(this);
        mRegistry.on_destroy<RigidbodyComponent>().connect<&Scene::OnRigidbodyDestroyed>(this);

        Physics* physics = Core::GetPhysics();
        physics->OptimizeBroadPhase();
    }

    void Scene::OnRuntimeEnd()
    {
//         {
//             auto view = mRegistry.view<ScriptComponent>();
//             for(auto entityID : view)
//                 OnScriptDestroyed(mRegistry, entityID);
//         }
//         {
//             auto view = mRegistry.view<UICanvasComponent>();
//             for(auto entityID : view)
//                 OnUICanvasDestroyed(mRegistry, entityID);
//         }
        mIsRunning = false;
    }

    void Scene::UpdateAudio()
    {
        SURGE_PROFILE_FUNC("Scene::UpdateAudio");
        if (!mIsRunning)
            return;

        auto listenerView = mRegistry.view<TransformComponent, AudioListenerComponent>();
        AudioEngine* audioEngine = Core::GetAudioEngine();

        // Listener
        for(auto entity : listenerView)
        {
            auto& transform = listenerView.get<TransformComponent>(entity);
            audioEngine->SetListenerPosition(transform.Position.x, transform.Position.y, transform.Position.z);
            break; // Grab the first listener we find, since there should only be one atp
        }

        // Audio Sources
        auto sourceView = mRegistry.view<TransformComponent, AudioSourceComponent>();
        for(auto [entity, transform, audioSrc] : sourceView.each())
        {
            Ref<Audio> asset = audioSrc.AudioClip.As<Audio>();
            if(asset)
            {
                if(!audioSrc.IsInitialized)
                {
                    audioSrc.RuntimeID = audioEngine->CreateAudio(asset->GetFilepath(), audioSrc.IsStreaming, audioSrc.IsSpatialized);
                    audioEngine->SetVolume(audioSrc.RuntimeID, audioSrc.Volume);
                    audioEngine->SetPitch(audioSrc.RuntimeID, audioSrc.Pitch);
                    audioEngine->SetLooping(audioSrc.RuntimeID, audioSrc.Loop);
                    audioEngine->SetMinDistance(audioSrc.RuntimeID, audioSrc.MinDistance);
                    audioEngine->SetMaxDistance(audioSrc.RuntimeID, audioSrc.MaxDistance);
                    audioEngine->SetAttenuationModel(audioSrc.RuntimeID, audioSrc.Attenuation);

                    if(audioSrc.PlayOnAwake)
                    {
                        audioEngine->PlayAudio(audioSrc.RuntimeID);
                        audioSrc.IsPlaying = true;
                    }
                    audioSrc.IsInitialized = true;
                }

                // Sync 3D Position
                if(audioSrc.IsSpatialized && audioSrc.RuntimeID)
                    audioEngine->SetPosition(transform.Position.x, transform.Position.y, transform.Position.z, audioSrc.RuntimeID);
            }
        }
    }

    void Scene::UpdateRendering(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec2& cameraNearFar)
    {
        SURGE_PROFILE_FUNC("Scene::UpdateRendering");

        Renderer* renderer = Core::GetRenderer();
        renderer->BeginFrame(viewMatrix, projectionMatrix, cameraNearFar);
        {
            auto view = mRegistry.view<SpriteRendererComponent, TransformComponent>();
            for(const auto& [entity, sprite, transform] : view.each())
            {
                if(!sprite.Active)
                    continue;

                ImageHandle textureHandle = sprite.TextureAsset ? sprite.TextureAsset.As<Texture2D>()->GetRHIImage() : ImageHandle::Invalid();
                renderer->SubmitQuad(transform.GetTransform(), sprite.Color, sprite.Billboard, textureHandle);
            }

            auto txtView = mRegistry.view<TextComponent, TransformComponent>();
            for(const auto& [entity, txtCmp, transform] : txtView.each())
            {
                if (!txtCmp.Active)
                    continue;

                if (txtCmp.FontAsset)
                {
                    Ref<Font> font = txtCmp.FontAsset.As<Font>();
                    renderer->SubmitText(transform.GetTransform(), txtCmp.Text, txtCmp.Color, txtCmp.MaxWidth, txtCmp.LetterSpacing, txtCmp.LineSpacing, 
                                         txtCmp.Alignment, txtCmp.VerticalAlignment, txtCmp.Italic, txtCmp.Underline, txtCmp.ShadowEnabled, txtCmp.ShadowOffset, txtCmp.ShadowColor, font.Raw(), txtCmp.Billboard);
                }
            }
        }
        {
            auto view = mRegistry.view<LightComponent, TransformComponent>();
            for(const auto& [entity, light, transform] : view.each())
            {
                if(!light.Active)
                    continue;

                Light gpuLight {};
                gpuLight.Color = light.Color;
                gpuLight.Intensity = light.Intensity;
                gpuLight.Radius = light.Radius;
                gpuLight.Falloff = light.Falloff;

                if(light.Type == LightType::DIRECTIONAL)
                {
                    glm::vec3 dirLightDir = transform.GetTransform()[2];
                    gpuLight.PositionType = glm::vec4(dirLightDir, 0.0f); // w = 0.0f for dir light

                    if(!mIsRunning)
                    {
                        glm::vec3 forwardDir = glm::normalize(glm::vec3(transform.GetTransform()[2]));
                        glm::vec4 debugColor = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f);
                        renderer->SubmitDirLightDebug(transform.Position, forwardDir, debugColor);
                    }
                }
                else if(light.Type == LightType::POINT)
                    gpuLight.PositionType = glm::vec4((glm::vec3)transform.GetTransform()[3], 1.0f); // w = 1.0f for point lights

                renderer->SubmitLight(gpuLight);
            }
        }
        {
            auto view = mRegistry.view<EnvironmentComponent>();
            for(const auto& [entity, env] : view.each())
            {
                if(!env.Active)
                    continue;

                Environnment e {};
                e.Elevation = env.Elevation;
                e.Azimuth = env.Azimuth;
                e.Turbidity = env.Turbidity;
                e.Exposure = env.Exposure;
                e.SunIntensity = env.SunIntensity;
                e.EnableSunDisk = env.EnableSunDisk;
                e.SkyAmbient = env.SkyAmbient;
                e.HorizonAmbient = env.HorizonAmbient;
                e.GroundAmbient = env.GroundAmbient;
                renderer->SubmitEnvironment(std::move(e));
                break; // Only submit the first environment component we find
            }
        }
        {
            // 3D Meshes
            auto meshGroup = mRegistry.group<MeshComponent>(entt::get<TransformComponent>);
            for(const auto& [entity, meshComponent, transformComponent] : meshGroup.each())
            {
                if(!meshComponent.Active)
                    continue;

                if(meshComponent.MeshAsset)
                {
                    Ref<Mesh> mesh = meshComponent.MeshAsset.As<Mesh>();
                    if(mesh) //Asset might be missing/corrupted, so check before submitting
                        renderer->SubmitMesh(transformComponent.GetTransform(), mesh.Raw(), meshComponent.DropShadow);
                }
            }

        }

        // DEBUG
        if(sSelectedEntity && mRenderDebug)
        {
            if(const MeshComponent* meshComp = sSelectedEntity.TryGetComponent<MeshComponent>())
            {
                if(meshComp->MeshAsset)
                {
                    Ref<Mesh> mesh = meshComp->MeshAsset.As<Mesh>();
                    if(mesh) //Asset might be missing/corrupted, so check before submitting
                    {
                        const glm::mat4& transform = sSelectedEntity.GetComponent<TransformComponent>().GetTransform();
                        renderer->SubmitMeshOutline(transform, mesh.Raw());
                    }
                }
            }

            // No Collider showing for MeshColliderComponent
            if(mRegistry.any_of<BoxColliderComponent, SphereColliderComponent, CapsuleColliderComponent, CylinderColliderComponent, ConvexColliderComponent>(sSelectedEntity))
            {
                bool showCollider = false;
                if(sSelectedEntity.HasComponent<BoxColliderComponent>())
                    showCollider = sSelectedEntity.GetComponent<BoxColliderComponent>().ShowCollider;
                else if(sSelectedEntity.HasComponent<SphereColliderComponent>())
                    showCollider = sSelectedEntity.GetComponent<SphereColliderComponent>().ShowCollider;
                else if(sSelectedEntity.HasComponent<CapsuleColliderComponent>())
                    showCollider = sSelectedEntity.GetComponent<CapsuleColliderComponent>().ShowCollider;
                else if(sSelectedEntity.HasComponent<CylinderColliderComponent>())
                    showCollider = sSelectedEntity.GetComponent<CylinderColliderComponent>().ShowCollider;
                else if(sSelectedEntity.HasComponent<ConvexColliderComponent>())
                    showCollider = sSelectedEntity.GetComponent<ConvexColliderComponent>().ShowCollider;

                if(showCollider)
                {
                    auto& transformComp = sSelectedEntity.GetComponent<TransformComponent>();
                    static JPH::ShapeRefC sTempShape = nullptr;

                    JPH::ShapeRefC shape;
                    if(sSelectedEntity.HasComponent<ConvexColliderComponent>())
                    {
                        ConvexColliderComponent& convexComp = sSelectedEntity.GetComponent<ConvexColliderComponent>();
                        if(!sTempShape || convexComp.IsDirty)
                        {
                            sTempShape = Core::GetPhysics()->CreateShape(sSelectedEntity);
                            convexComp.IsDirty = false;
                        }
                        shape = sTempShape;
                    }
                    else
                        shape = Core::GetPhysics()->CreateShape(sSelectedEntity);

                    if(shape)
                    {
                        JPH::Vec3 comOffset = shape->GetCenterOfMass();
                        JPH::Vec3 joltPosition = JPH::Vec3(transformComp.Position.x, transformComp.Position.y, transformComp.Position.z);

                        glm::quat q = glm::quat(glm::radians(transformComp.Rotation));
                        JPH::Quat joltRotation = JPH::Quat(q.x, q.y, q.z, q.w);

                        joltPosition = joltPosition + joltRotation * comOffset;
                        JPH::RMat44 joltTransform = JPH::RMat44::sRotationTranslation(joltRotation, joltPosition);

                        JPH::DebugRenderer* debugRenderer = Core::GetPhysics()->GetDebugRenderer();
                        shape->Draw(debugRenderer, joltTransform, JPH::Vec3::sReplicate(1.0f), JPH::Color::sGreen, false, true);
                    }
                }
            }
            if(sSelectedEntity.HasComponent<AudioSourceComponent>())
            {
                auto& audioSrc = sSelectedEntity.GetComponent<AudioSourceComponent>();
                auto& transform = sSelectedEntity.GetComponent<TransformComponent>();

                if(audioSrc.IsSpatialized)
                {
                    const int segments = 32;
                    const float pi = 3.14159265359f;

                    // LIFT the circles 0.1f units above the entity so they don't clip into the floor!
                    glm::vec3 center = transform.Position + glm::vec3(0.0f, 0.1f, 0.0f);

                    for(int i = 0; i < segments; ++i)
                    {
                        float theta1 = (float)i / segments * 2.0f * pi;
                        float theta2 = (float)(i + 1) / segments * 2.0f * pi;

                        // --- MIN DISTANCE (GREEN CIRCLE) ---
                        glm::vec3 minP0 = center + glm::vec3(std::cos(theta1) * audioSrc.MinDistance, 0.0f, std::sin(theta1) * audioSrc.MinDistance);
                        glm::vec3 minP1 = center + glm::vec3(std::cos(theta2) * audioSrc.MinDistance, 0.0f, std::sin(theta2) * audioSrc.MinDistance);

                        renderer->SubmitLine(minP0, minP1, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));

                        // --- MAX DISTANCE (RED CIRCLE) ---
                        glm::vec3 maxP0 = center + glm::vec3(std::cos(theta1) * audioSrc.MaxDistance, 0.0f, std::sin(theta1) * audioSrc.MaxDistance);
                        glm::vec3 maxP1 = center + glm::vec3(std::cos(theta2) * audioSrc.MaxDistance, 0.0f, std::sin(theta2) * audioSrc.MaxDistance);

                        renderer->SubmitLine(maxP0, maxP1, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
                    }
                }
            }
        }
        renderer->EndFrame();
    }

    void Scene::Update(EditorCamera& camera)
    {
        SURGE_PROFILE_FUNC("Scene::Update(Editor)");

        UpdateAudio();
        UpdatePhysics();
        UpdateScripts();
        UpdateTransforms();
        UpdateRendering(camera.GetViewMatrix(), camera.GetProjectionMatrix(), camera.GetNearAndFarPlane());
    }

    void Scene::Update()
    {
        SURGE_PROFILE_FUNC("Scene::Update()");

        // Order matters here
        UpdateAudio();
        UpdatePhysics();
        UpdateScripts();
        UpdateTransforms();

        Pair<RuntimeCamera*, glm::mat4> camera = GetMainCameraEntity();
        if(camera.Data1)
            UpdateRendering(glm::inverse(camera.Data2), camera.Data1->GetProjectionMatrix(), { camera.Data1->GetPerspectiveNearClip(), camera.Data1->GetPerspectiveFarClip() });
    }

    template <typename T>
    static void CopyComponent(entt::registry& dstRegistry, entt::registry& srcRegistry, const std::unordered_map<UUID, entt::entity>& enttMap)
    {
        auto components = srcRegistry.view<T>();
        for(entt::entity srcEntity : components)
        {
            entt::entity destEntity = enttMap.at(srcRegistry.get<IDComponent>(srcEntity).ID);
            auto& srcComponent = srcRegistry.get<T>(srcEntity);

            // We need to preserve the entt::entity to point to correct entity while copying
            if constexpr(std::is_same_v<T, RelationshipComponent>)
            {
                RelationshipComponent destComponent;

                auto MapEntity = [&](Uint oldEntUint) -> Uint {
                    if(oldEntUint == RELATIONSHIP_NULL)
                        return RELATIONSHIP_NULL;

                    entt::entity oldEnt = static_cast<entt::entity>(oldEntUint);
                    UUID oldUUID = srcRegistry.get<IDComponent>(oldEnt).ID;
                    entt::entity newEnt = enttMap.at(oldUUID);
                    return static_cast<Uint>(newEnt);
                    };

                destComponent.Parent = MapEntity(srcComponent.Parent);
                destComponent.FirstChild = MapEntity(srcComponent.FirstChild);
                destComponent.PreviousSibling = MapEntity(srcComponent.PreviousSibling);
                destComponent.NextSibling = MapEntity(srcComponent.NextSibling);
                destComponent.ChildrenCount = srcComponent.ChildrenCount;

                dstRegistry.emplace_or_replace<T>(destEntity, destComponent);
            }
            else
            {
                dstRegistry.emplace_or_replace<T>(destEntity, srcComponent);
            }
        }
    }

    void Scene::CopyTo(Scene* other)
    {
        other->mRegistry.clear();

        std::unordered_map<UUID, entt::entity> enttMap;
        auto idComponents = mRegistry.view<IDComponent>();
        for (entt::entity entity : idComponents)
        {
            UUID uuid = mRegistry.get<IDComponent>(entity).ID;
            Entity e;
            other->CreateEntityWithID(e, uuid, "");
            enttMap[uuid] = e.Raw();
        }

        CopyComponent<NameComponent>(other->mRegistry, mRegistry, enttMap);
        CopyComponent<RelationshipComponent>(other->mRegistry, mRegistry, enttMap);
        CopyComponent<TransformComponent>(other->mRegistry, mRegistry, enttMap);
        CopyComponent<SpriteRendererComponent>(other->mRegistry, mRegistry, enttMap);
        CopyComponent<MeshComponent>(other->mRegistry, mRegistry, enttMap);
        CopyComponent<CameraComponent>(other->mRegistry, mRegistry, enttMap);
        CopyComponent<LightComponent>(other->mRegistry, mRegistry, enttMap);
        CopyComponent<EnvironmentComponent>(other->mRegistry, mRegistry, enttMap);
        CopyComponent<TextComponent>(other->mRegistry, mRegistry, enttMap);

        CopyComponent<RigidbodyComponent>(other->mRegistry, mRegistry, enttMap);
        CopyComponent<BoxColliderComponent>(other->mRegistry, mRegistry, enttMap);
        CopyComponent<SphereColliderComponent>(other->mRegistry, mRegistry, enttMap);
        CopyComponent<CapsuleColliderComponent>(other->mRegistry, mRegistry, enttMap);
        CopyComponent<CylinderColliderComponent>(other->mRegistry, mRegistry, enttMap);
        CopyComponent<ConvexColliderComponent>(other->mRegistry, mRegistry, enttMap);
        CopyComponent<MeshColliderComponent>(other->mRegistry, mRegistry, enttMap);

        CopyComponent<ScriptComponent>(other->mRegistry, mRegistry, enttMap);
        CopyComponent<UICanvasComponent>(other->mRegistry, mRegistry, enttMap);

        CopyComponent<AudioListenerComponent>(other->mRegistry, mRegistry, enttMap);
        CopyComponent<AudioSourceComponent>(other->mRegistry, mRegistry, enttMap);
    }

    Surge::Entity Scene::FindEntityByUUID(UUID id)
    {
        auto view = mRegistry.view<IDComponent>();
        for (const auto& entity : view)
        {
            auto& idComponent = mRegistry.get<IDComponent>(entity);
            if (idComponent.ID == id)
                return Entity(entity, this);
        }

        return Entity {};
    }

    void Scene::CreateEntity(Entity& outEntity, const String& name)
    {
        entt::entity e = mRegistry.create();
        outEntity = Entity(e, this);
        outEntity.AddComponent<IDComponent>();
        outEntity.AddComponent<NameComponent>(name);
        outEntity.AddComponent<TransformComponent>();
        outEntity.AddComponent<RelationshipComponent>();
    }

    void Scene::CreateEntityEmpty(Entity& outEntity, const String& name)
    {
        entt::entity e = mRegistry.create();
        outEntity = Entity(e, this);
        outEntity.AddComponent<IDComponent>();
        outEntity.AddComponent<NameComponent>(name);
        outEntity.AddComponent<RelationshipComponent>();
    }

    void Scene::CreateEntityWithID(Entity& outEntity, const UUID& id, const String& name)
    {
        entt::entity e = mRegistry.create();
        outEntity = Entity(e, this);
        outEntity.AddComponent<IDComponent>(id);
        outEntity.AddComponent<NameComponent>(name);
        outEntity.AddComponent<TransformComponent>();
        outEntity.AddComponent<RelationshipComponent>();
    }

    void Scene::DestroyEntity(Entity entity)
    {
        if(!entity)
            return;

        if(entity.HasComponent<ScriptComponent>())
            OnScriptDestroyed(mRegistry, entity.Raw());

        auto& rel = entity.GetComponent<RelationshipComponent>();

        entt::entity currentChild = (entt::entity)rel.FirstChild;
        while(currentChild != entt::null)
        {
            Entity child(currentChild, this);
            entt::entity nextChild = (entt::entity)child.GetComponent<RelationshipComponent>().NextSibling;

            // (Every child will loop back to the top of this function and fire its own script callback)
            DestroyEntity(child);
            currentChild = nextChild;
        }

        SetParent(entity, Entity {});
        mRegistry.destroy(entity.Raw());
    }

    Entity Scene::DuplicateEntity(Entity entity)
    {
        if(entity.GetComponent<RelationshipComponent>().Parent != entt::null)
        {
            Log<Severity::Warn>("Scene::DuplicateEntity: Entity with a parent cannot be duplicated yet!");
            return Entity(entt::null, nullptr);
        }

        Entity newEntity;

        String originalName = entity.GetComponent<NameComponent>().Name;
        String newName = originalName;

        std::regex re("(.*) \\(([0-9]+)\\)$"); // Regex to check if the name already ends with " (Number)"
        std::smatch match;
        if(std::regex_match(originalName, match, re))
        {
            // match[1] -> Base name ("Cube")
            // match[2] -> Number ("1")
            String baseName = match[1].str();
            int currentNum = std::stoi(match[2].str());
            newName = std::format("{} ({})", baseName, currentNum + 1);
        }
        else
            newName = std::format("{} (1)", originalName);

        CreateEntityEmpty(newEntity, newName);

        auto CopyIfHas = [&](auto componentType) {
            using T = typename decltype(componentType)::type;
            if(entity.HasComponent<T>())
            {
                newEntity.AddComponent<T>(entity.GetComponent<T>());
            }
            };

        CopyIfHas(std::type_identity<TransformComponent>{});
        CopyIfHas(std::type_identity<MeshComponent>{});
        CopyIfHas(std::type_identity<RigidbodyComponent>{});
        CopyIfHas(std::type_identity<BoxColliderComponent>{});
        CopyIfHas(std::type_identity<SphereColliderComponent>{});
        CopyIfHas(std::type_identity<CapsuleColliderComponent>{});
        CopyIfHas(std::type_identity<CylinderColliderComponent>{});
        CopyIfHas(std::type_identity<ConvexColliderComponent>{});
        CopyIfHas(std::type_identity<MeshColliderComponent>{});
        CopyIfHas(std::type_identity<LightComponent>{});
        CopyIfHas(std::type_identity<SpriteRendererComponent>{});
        CopyIfHas(std::type_identity<TextComponent>{});
        CopyIfHas(std::type_identity<ScriptComponent>{});
        CopyIfHas(std::type_identity<UICanvasComponent>{});
        CopyIfHas(std::type_identity<AudioListenerComponent>{});
        CopyIfHas(std::type_identity<AudioSourceComponent>{});

        return newEntity;
    }

    void Scene::SetParent(Entity entity, Entity newParent)
    {
        if(entity == newParent)
            return;

        auto& rel = entity.GetComponent<RelationshipComponent>();

        // Unlink from current parent
        if(rel.Parent != entt::null)
        {
            Entity oldParent((entt::entity)rel.Parent, this);
            auto& oldParentRel = oldParent.GetComponent<RelationshipComponent>();

            // If we are the first child, point the parent to our next sibling
            if((entt::entity)oldParentRel.FirstChild == entity.Raw())
                oldParentRel.FirstChild = rel.NextSibling;

            // Bridge the gap between our previous and next siblings
            if(rel.PreviousSibling != entt::null)
                Entity((entt::entity)rel.PreviousSibling, this).GetComponent<RelationshipComponent>().NextSibling = rel.NextSibling;

            if(rel.NextSibling != entt::null)
                Entity((entt::entity)rel.NextSibling, this).GetComponent<RelationshipComponent>().PreviousSibling = rel.PreviousSibling;

            oldParentRel.ChildrenCount--;
        }

        // Link to new parent
        rel.Parent = newParent ? (Uint)newParent.Raw() : entt::null;
        rel.PreviousSibling = entt::null;
        rel.NextSibling = entt::null;
        if(newParent)
        {
            auto& newParentRel = newParent.GetComponent<RelationshipComponent>();

            if(newParentRel.FirstChild == entt::null)
                newParentRel.FirstChild = (Uint)entity.Raw(); // We are the first and only child
            else
            {
                // Walk the sibling chain to find the end, and attach ourselves
                entt::entity currentSibling = (entt::entity)newParentRel.FirstChild;
                while(true)
                {
                    auto& siblingRel = mRegistry.get<RelationshipComponent>(currentSibling);
                    if(siblingRel.NextSibling == entt::null)
                    {
                        siblingRel.NextSibling = (Uint)entity.Raw();
                        rel.PreviousSibling = (Uint)currentSibling;
                        break;
                    }
                    currentSibling = (entt::entity)siblingRel.NextSibling;
                }
            }
            newParentRel.ChildrenCount++;
        }
    }

    void Scene::SetSelectedEntity(Entity entity)
    {
        sSelectedEntity = entity;
    }

    Entity Scene::GetSelectedEntity() const
    {
        return sSelectedEntity;
    }

    void Scene::OnResize(float width, float height)
    {
        if (width <= 0 || height <= 0)
            return;

        auto view = mRegistry.view<CameraComponent>();
        for(auto entity : view)
        {
            auto& cameraComponent = view.get<CameraComponent>(entity);
            if(!cameraComponent.FixedAspectRatio)
                cameraComponent.Camera.SetViewportSize(width, height);
        }
    }

    Entity Scene::GetEntityByName(const String& name)
    {
        auto view = mRegistry.view<NameComponent>();
        for(auto entityID : view)
        {
            if(view.get<NameComponent>(entityID).Name == name)
                return Entity { entityID, this };
        }

        return Entity { entt::null, nullptr };
    }

    Pair<RuntimeCamera*, glm::mat4> Scene::GetMainCameraEntity()
    {
        Pair<RuntimeCamera*, glm::mat4> result = {nullptr, glm::mat4(1.0f)};
        auto view = mRegistry.view<TransformComponent, CameraComponent>();
        for (auto& entity : view)
        {
            const auto& [transform, camera] = view.get<TransformComponent, CameraComponent>(entity);
            if (camera.Primary && camera.Active)
            {
                result = {&camera.Camera, transform.GetTransform()};
                break;
            }
        }
        return result;
    }

    void Scene::UpdateTransforms()
    {
        SURGE_PROFILE_FUNC("Scene::UpdateTransforms");

        auto UpdateTransformHierarchy = [&](auto&& self, entt::entity entity, const glm::mat4& parentWorldTransform, bool parentDirty) -> void {
            auto& transform = mRegistry.get<TransformComponent>(entity);
            auto& rel = mRegistry.get<RelationshipComponent>(entity);
            bool isDirty = transform.IsDirty() || parentDirty;

            if(isDirty)
            {
                const glm::mat4& local = transform.GetLocalTransform();
                transform.SetWorldTransform(parentWorldTransform * local);
            }

            // Recursively propagate down to all children
            entt::entity currentChild = (entt::entity)rel.FirstChild;
            while(currentChild != entt::null)
            {
                self(self, currentChild, transform.GetTransform(), isDirty);
                currentChild = (entt::entity)mRegistry.get<RelationshipComponent>(currentChild).NextSibling;
            }
        };

        auto view = mRegistry.view<TransformComponent, RelationshipComponent>();
        for(auto entity : view)
        {
            const auto& rel = view.get<RelationshipComponent>(entity);
            if(rel.Parent == entt::null)
                UpdateTransformHierarchy(UpdateTransformHierarchy, entity, glm::mat4(1.0f), false);
        }
    }

    void Scene::UpdateScripts()
    {
        SURGE_PROFILE_FUNC("Scene::UpdateScripts");

        if(mIsRunning)
        {
            {
                auto view = mRegistry.view<ScriptComponent>();
                for(auto entityID : view)
                {
                    auto& scriptComp = view.get<ScriptComponent>(entityID);
                    if(!scriptComp.ScriptAsset || !scriptComp.Active)
                        continue;

                    Ref<Script> script = scriptComp.ScriptAsset.As<Script>();
                    Entity entityObj = { entityID, this };
                    if(!scriptComp.IsInstantiated)
                    {
                        script->CreateEnvironment();
                        script->ExecuteOnCreate(entityObj);
                        scriptComp.IsInstantiated = true;
                    }
                    else
                        script->ExecuteOnUpdate(entityObj);
                }
            }
            {
                auto view = mRegistry.view<UICanvasComponent>();
                for(auto entityID : view)
                {
                    auto& uiComp = view.get<UICanvasComponent>(entityID);
                    if(!uiComp.ScriptAsset || !uiComp.Active)
                        continue;

                    Core::GetRenderer()->ShowUI(uiComp.ShowCanvas); // (Rid) This should be here?

                    Ref<Script> script = uiComp.ScriptAsset.As<Script>();
                    Entity entityObj = { entityID, this };
                    if(uiComp.ShowCanvas)
                    {
                        if(!uiComp.IsInstantiated)
                        {
                            script->CreateEnvironment();
                            script->ExecuteOnCreate(entityObj);
                            uiComp.IsInstantiated = true;
                        }
                        else if(uiComp.IsInstantiated)
                            script->ExecuteOnUpdate(entityObj);
                    }
                }
            }

            // Handle Physics Collisions
            auto collisions = Core::GetPhysics()->GetContactListener().FlushEvents();
            for(const auto& event : collisions)
            {
                Entity entA(static_cast<entt::entity>(event.EnttIDA), this);
                Entity entB(static_cast<entt::entity>(event.EnttIDB), this);

                if(!mRegistry.valid(entA) || !mRegistry.valid(entB))
                    continue;

                // Fire for Entity A
                if(entA.HasComponent<ScriptComponent>())
                {
                    auto& scriptC = entA.GetComponent<ScriptComponent>();
                    if (scriptC.ScriptAsset)
                        scriptC.ScriptAsset.As<Script>()->ExecuteOnCollisionEnter(entA, entB);
                }

                // Fire for Entity B
                if(entB.HasComponent<ScriptComponent>())
                {
                    auto& scriptC = entB.GetComponent<ScriptComponent>();
                    if(scriptC.ScriptAsset)
                        scriptC.ScriptAsset.As<Script>()->ExecuteOnCollisionEnter(entB, entA);
                }
            }

        }
    }

    void Scene::UpdatePhysics()
    {
        SURGE_PROFILE_FUNC("Scene::UpdatePhysics");

        if(mIsRunning)
        {
            Physics* physics = Core::GetPhysics();
            auto view = mRegistry.view<TransformComponent, RigidbodyComponent>();
            for(auto [entity, transformComp, rb] : view.each())
            {
                if(physics->IsInValid(rb.RuntimeBodyID) || !physics->IsActive(rb.RuntimeBodyID) || !rb.Active)
                    continue;

                transformComp.Position = physics->GetPosition(rb.RuntimeBodyID);
                transformComp.Rotation = physics->GetRotation(rb.RuntimeBodyID);
                transformComp.MarkDirty(); // Fucking fuck ass MarkDirty funciton, I forogt to add this and spent 30mins debugging why my Physics system is not updating
            }
        }
    }

    void Scene::AddStartupEntities()
    {
        // Add default perspective camera
        Entity runtimeCamera;
        CreateEntity(runtimeCamera, "Runtime Camera");
        CameraComponent& cam = runtimeCamera.AddComponent<CameraComponent>();
        cam.Primary = true;
        cam.FixedAspectRatio = true;
        cam.Camera.SetProjectionType(RuntimeCamera::ProjectionType::Perspective);
        TransformComponent& transform = runtimeCamera.GetComponent<TransformComponent>();
        transform.Position = glm::vec3(-10, 6, 10);
        transform.Rotation = glm::vec3(-30, -45, 0);

        glm::vec2 windowSize = Core::GetWindow()->GetSize();
        OnResize(windowSize.x, windowSize.y);

        AssetManager* assetManager = Core::GetAssetManager();
        {
            Entity e;
            CreateEntity(e, "Cube");
            MeshComponent& meshComp = e.AddComponent<MeshComponent>();
            AssetID kek = assetManager->Import(DefaultMesh::CUBE, AssetType::MESH);
            meshComp.MeshAsset = assetManager->Load<Mesh>(kek);

            TransformComponent& t = e.GetComponent<TransformComponent>();
            t.Position = glm::vec3(0.0f, 2.5f, 0.0f);
            t.Rotation = glm::vec3(45.0f, 60.0f, 20.0f);
            t.Scale = glm::vec3(1.0f, 1.0f, 1.0f);
            t.MarkDirty();
        }
        {
            Entity floor;
            CreateEntity(floor, "Floor");
            MeshComponent& meshComp = floor.AddComponent<MeshComponent>();
            AssetID kek = assetManager->Import(DefaultMesh::CYLINDER, AssetType::MESH);
            meshComp.MeshAsset = assetManager->Load<Mesh>(kek);

            TransformComponent& t = floor.GetComponent<TransformComponent>();
            t.Position = glm::vec3(0.0f, 0.0f, 0.0f);
            t.Scale = glm::vec3(15.0f, 1.0f, 15.0f);
            t.MarkDirty();
        }
        {
            Entity directionalLight;
            CreateEntity(directionalLight, "Directional Light");
            LightComponent& lightComp = directionalLight.AddComponent<LightComponent>();
            lightComp.Type = LightType::DIRECTIONAL;
            lightComp.Intensity = 5.5f;
            lightComp.Radius = 1.0f;
            TransformComponent& t = directionalLight.GetComponent<TransformComponent>();
            t.Position = glm::vec3(0.0f, 0.0f, 0.0f);
            t.Rotation = glm::vec3(30.0f, -30.0f, 30.0f);
            t.MarkDirty();
        }
        {
            Entity env;
            CreateEntity(env, "Environemnt");
            env.AddComponent<EnvironmentComponent>();
        }
    }

    void Scene::OnColliderAdded(entt::registry& registry, entt::entity entity)
    {
        if(!registry.all_of<RigidbodyComponent, TransformComponent>(entity) ||
           !(registry.any_of<BoxColliderComponent, SphereColliderComponent, CapsuleColliderComponent, CylinderColliderComponent, ConvexColliderComponent, MeshColliderComponent>(entity)))
            return;

        Core::GetPhysics()->CreateRigidbody(Entity(entity, this));
    }

    void Scene::OnRigidbodyDestroyed([[maybe_unused]] entt::registry& registry, entt::entity entity)
    {
        Core::GetPhysics()->DestroyRigidbody(Entity(entity, this));
    }

    void Scene::OnScriptDestroyed(entt::registry&, entt::entity entity)
    {
        Entity e(entity, this);
        ScriptComponent& comp = e.GetComponent<ScriptComponent>();
        if(comp.IsInstantiated)
        {
            comp.ScriptAsset.As<Script>()->ExecuteOnDestroy(e);
            comp.IsInstantiated = false;
        }
    }
    void Scene::OnUICanvasDestroyed(entt::registry&, entt::entity entity)
    {
        Entity e(entity, this);
        UICanvasComponent& comp = e.GetComponent<UICanvasComponent>();
        if(comp.IsInstantiated)
        {
            comp.ScriptAsset.As<Script>()->ExecuteOnDestroy(e);
            comp.IsInstantiated = false;
        }
    }

    void Scene::OnAudioSourceComponentDestroyed(entt::registry&, entt::entity entity)
    {
        Entity e(entity, this);
        AudioSourceComponent& comp = e.GetComponent<AudioSourceComponent>();
        if(comp.IsInitialized && comp.RuntimeID)
            Core::GetAudioEngine()->DestroyAudio(comp.RuntimeID);
    }

} // namespace Surge
