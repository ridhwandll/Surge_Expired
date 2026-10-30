// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/ECS/Scene.hpp"
#include "Surge/ECS/Components/Components.hpp"
#include "Surge/ECS/Components/ScriptComponent.hpp"

#include "Surge/Core/Profiler.hpp"
#include "Surge/Core/Window/Window.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Graphics/HighLevel/Mesh.hpp"
#include "Surge/Graphics/HighLevel/DefaultMeshes.hpp"
#include "Surge/Physics/Physics.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/ScriptEngine/ScriptAsset.hpp"
#include "Surge/ScriptEngine/ScriptEngine.hpp"
#include "Surge/Core/Time/Clock.hpp"
#include "Surge/Graphics/UISystem/UILayout.hpp"

#include <regex>
#include "Surge/Audio/AudioEngine.hpp"
#include "Surge/Audio/Audio.hpp"

namespace Surge
{
    Scene::Scene(const String& name, const SceneContext& context)
        : mName(name), mContext(context)
    {
        AddStartupEntities();

        // Are these good here?
        mRegistry.on_destroy<ScriptComponent>().connect<&Scene::OnScriptDestroyed>(this);
        mRegistry.on_destroy<UICanvasComponent>().connect<&Scene::OnUICanvasDestroyed>(this);
        mRegistry.on_destroy<AudioSourceComponent>().connect<&Scene::OnAudioSourceComponentDestroyed>(this);
    }

    Scene::~Scene()
    {
        if(Physics* physics = mContext.Physics_)
            physics->ClearFixedStepCallback(this);
        mRegistry.clear();
    }

    void Scene::OnRuntimeStart()
    {
        mIsRunning = true;

        // Refresh the asset references from the asset manager to ensure it's loaded and valid
        for(const auto& [entity, scripts] : mRegistry.view<ScriptComponent>().each())
        {
            for(ScriptEntry& entry : scripts.Scripts)
            {
                if(entry.ScriptAsset)
                    entry.ScriptAsset = mContext.AssetManager_->Load<Script>(entry.ScriptAsset->GetID());
            }
        }
        for(const auto& [entity, canvas] : mRegistry.view<UICanvasComponent>().each())
        {
            if (canvas.ScriptAsset)
                canvas.ScriptAsset = mContext.AssetManager_->Load<Script>(canvas.ScriptAsset->GetID());
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

        Physics* physics = mContext.Physics_;
        physics->OptimizeBroadPhase();

        // Contacts of a previous session must not reach this one, OnFixedUpdate runs before every physics step from now on
        physics->GetContactListener().Reset();
        physics->SetFixedStepCallback(this, [this](float fixedDeltaTime) { FixedUpdateScripts(fixedDeltaTime); });
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
        mContext.Physics_->ClearFixedStepCallback(this);
        mContext.Physics_->ClearShapeCache(); // The next scene builds what it uses, live bodies keep their shapes
        mIsRunning = false;
    }

    void Scene::DestroyEntityImmediate(Entity entity)
    {
        if(entity.HasComponent<ScriptComponent>())
            OnScriptDestroyed(mRegistry, entity.Raw());

        auto& rel = entity.GetComponent<RelationshipComponent>();

        entt::entity currentChild = (entt::entity)rel.FirstChild;
        while(currentChild != entt::null)
        {
            Entity child(currentChild, this);
            entt::entity nextChild = (entt::entity)child.GetComponent<RelationshipComponent>().NextSibling;

            // (Every child will loop back to the top of this function and fire its own script callback)
            DestroyEntityImmediate(child);
            currentChild = nextChild;
        }

        SetParent(entity, Entity {});
        mRegistry.destroy(entity.Raw());
    }

    void Scene::UpdateAudio()
    {
        SURGE_PROFILE_FUNC("Scene::UpdateAudio");
        if (!mIsRunning)
            return;

        auto listenerView = mRegistry.view<TransformComponent, AudioListenerComponent>();
        AudioEngine* audioEngine = mContext.AudioEngine_;

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

            // The clip was swapped (Lua SetAudioClip, the Inspector): the sound still plays the previous one, rebuild it (playing again if it was)
            bool resumePlaying = false;
            if(audioSrc.RuntimeID && audioEngine->GetClip(audioSrc.RuntimeID) != asset.Raw())
            {
                resumePlaying = audioSrc.IsPlaying;
                audioEngine->DestroyAudio(audioSrc.RuntimeID);
                audioSrc.RuntimeID = nullptr;
                audioSrc.IsInitialized = false;
                audioSrc.IsPlaying = false;
            }

            if(asset)
            {
                if(!audioSrc.IsInitialized)
                {
                    // A clip that fails to play stays initialized without a sound
                    audioSrc.RuntimeID = audioEngine->CreateAudio(asset, audioSrc.IsStreaming, audioSrc.IsSpatialized);
                    if(audioSrc.RuntimeID)
                    {
                        audioEngine->SetVolume(audioSrc.RuntimeID, audioSrc.Volume);
                        audioEngine->SetPitch(audioSrc.RuntimeID, audioSrc.Pitch);
                        audioEngine->SetLooping(audioSrc.RuntimeID, audioSrc.Loop);
                        audioEngine->SetMinDistance(audioSrc.RuntimeID, audioSrc.MinDistance);
                        audioEngine->SetMaxDistance(audioSrc.RuntimeID, audioSrc.MaxDistance);
                        audioEngine->SetAttenuationModel(audioSrc.RuntimeID, audioSrc.Attenuation);

                        if(audioSrc.PlayOnAwake || resumePlaying)
                        {
                            audioEngine->PlayAudio(audioSrc.RuntimeID);
                            audioSrc.IsPlaying = true;
                        }
                    }
                    audioSrc.IsInitialized = true;
                }

                // Sync 3D Position
                if(audioSrc.IsSpatialized && audioSrc.RuntimeID)
                    audioEngine->SetPosition(transform.Position.x, transform.Position.y, transform.Position.z, audioSrc.RuntimeID);
            }
        }
    }

    void Scene::UpdateParticles(float deltaTime, const Frustum* frustum, bool submit)
    {
        SURGE_PROFILE_FUNC("Scene::UpdateParticles");

        Renderer* renderer = mContext.Renderer_;
        FrameBlackboard& blackBoard = renderer->GetRenderGraphBlackBoard();

        ParticleBudget budget;
        budget.MaxParticles = blackBoard.Particles.MaxParticles;
        budget.EmissionScale = glm::clamp(blackBoard.Particles.EmissionQuality, ParticleSettings::MIN_EMISSION_QUALITY, 1.0f);

        ParticleStats stats;
        auto view = mRegistry.view<ParticleSystemComponent, TransformComponent>();
        for(const auto& [entity, particles, transform] : view.each())
        {
            if(!particles.Active)
                continue;

            // AutoPlay effects also play in the Editor's edit scene (a live preview), the copy made for Play mode starts over
            particles.Runtime.Update(particles, transform.GetTransform(), deltaTime, frustum, budget);
            stats.Systems++;
            if(particles.Runtime.IsCulled())
                stats.CulledSystems++;

            if(submit)
                particles.Runtime.Submit(*renderer);

            if(mIsRunning && particles.AutoDestroy && particles.Runtime.IsFinished())
                DestroyEntity(Entity(entity, this)); // Deferred to the frame end
        }

        stats.AliveParticles = budget.Used;
        blackBoard.ParticleStatistics = stats;
    }

    void Scene::UpdateAnimation(float deltaTime, const Frustum* frustum)
    {
        SURGE_PROFILE_FUNC("Scene::UpdateAnimation");

        AnimationStats stats;
        auto view = mRegistry.view<AnimatorComponent, MeshComponent, TransformComponent>();
        for(const auto& [entity, animator, meshComponent, transform] : view.each())
        {
            if(!animator.Active || !meshComponent.Active || !meshComponent.MeshAsset)
                continue;

            animator.Runtime.Update(animator, meshComponent.MeshAsset.As<Mesh>(), transform.GetTransform(), deltaTime, frustum, mIsRunning);
            stats.Animators++;

            if(animator.Runtime.IsCulled())
                stats.Culled++;
        }

        // OverBudget is counted by the Renderer while the meshes submit, later this frame
        mContext.Renderer_->GetRenderGraphBlackBoard().AnimationStatistics = stats;
    }

    void Scene::UpdateRendering(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec2& cameraNearFar, float scaledDeltaTime, bool cull, const std::function<void(Scene&)>* editorDraw)
    {
        SURGE_PROFILE_FUNC("Scene::UpdateRendering");

        Renderer* renderer = mContext.Renderer_;
        renderer->BeginFrame(viewMatrix, projectionMatrix, cameraNearFar);

        // First active one wins
        {
            const RenderProfile profile = renderer->GetRenderProfile();
            const RenderSettings* settings = &RenderSettings::GetDefault(profile);
            for(const auto& [entity, component] : mRegistry.view<RendererSettingsComponent>().each())
            {
                if(component.Active)
                {
                    settings = &component.GetProfileSettings(profile);
                    break;
                }
            }
            renderer->SubmitRenderSettings(*settings);
        }

        // After BeginFrame: culled with this frame's camera (transforms are final atp)
        UpdateParticles(scaledDeltaTime, cull ? &renderer->GetRenderGraphBlackBoard().CameraFrustum : nullptr, true);
        UpdateAnimation(scaledDeltaTime, cull ? &renderer->GetRenderGraphBlackBoard().CameraFrustum : nullptr);

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
                                         txtCmp.Alignment, txtCmp.VerticalAlignment, txtCmp.Italic, txtCmp.Underline, txtCmp.ShadowEnabled, txtCmp.ShadowOffset,
                                         txtCmp.ShadowColor, font.Raw(), txtCmp.Billboard);
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
                    if(!mesh) //Asset might be missing/corrupted, so check before submitting
                        continue;

                    // Skinned meshes draw the animator's pose, the rest pose without one (SubmitMesh)
                    AnimatorComponent* animator = mesh->IsSkinned() ? mRegistry.try_get<AnimatorComponent>(entity) : nullptr;
                    if(animator && animator->Active && animator->Runtime.HasPose() && animator->Runtime.GetMesh() == mesh.Raw())
                        animator->Runtime.Submit(*renderer, transformComponent.GetTransform(), meshComponent.DropShadow);
                    else
                        renderer->SubmitMesh(transformComponent.GetTransform(), mesh.Raw(), meshComponent.DropShadow);
                }
            }

        }

        // Editor-only debug geometry (colliders)
        if(editorDraw && *editorDraw)
            (*editorDraw)(*this);

        renderer->EndFrame();
    }

    void Scene::ProcessPendingEntityDestructions()
    {
        if(mEntitiesToDestroy.empty())
            return;

        // Copy and clear to allow safe re-entrant destruction if needed
        Vector<entt::entity> toDestroy = std::move(mEntitiesToDestroy);
        mEntitiesToDestroy.clear();

        for(entt::entity raw : toDestroy)
        {
            if(mRegistry.valid(raw))
                DestroyEntityImmediate(Entity(raw, this));
        }
    }

    void Scene::Update(EditorCamera& camera, const std::function<void(Scene&)>& editorDraw)
    {
        SURGE_PROFILE_FUNC("Scene::Update(Editor)");

        const float scaledDeltaTime = UpdateSimulation();
        UpdateTransforms();
        // No particle culling: the Editor draws the scene through several views with their own cameras
        UpdateRendering(camera.GetViewMatrix(), camera.GetProjectionMatrix(), camera.GetNearAndFarPlane(), scaledDeltaTime, false, &editorDraw);
        ProcessPendingEntityDestructions();
    }

    float Scene::UpdateSimulation()
    {
        if(mIsPaused)
        {
            if(mStepFrames == 0)
                return 0.0f;
            mStepFrames--;
        }

        // Order matters here
        UpdateAudio();
        UpdatePhysics();
        UpdateScripts();

        // Scaled game time while running
        const float deltaTime = mContext.Clock_->GetSeconds();
        return mIsRunning ? deltaTime * mContext.Clock_->GetTimeScale() : deltaTime;
    }

    void Scene::Update()
    {
        SURGE_PROFILE_FUNC("Scene::Update()");

        const float scaledDeltaTime = UpdateSimulation();
        UpdateTransforms();

        Pair<RuntimeCamera*, glm::mat4> camera = GetMainCameraEntity();
        if(camera.Data1)
            UpdateRendering(glm::inverse(camera.Data2), camera.Data1->GetProjectionMatrix(), { camera.Data1->GetPerspectiveNearClip(), camera.Data1->GetPerspectiveFarClip() }, scaledDeltaTime, true);
        else
        {
            UpdateParticles(scaledDeltaTime, nullptr, false); // Nothing is drawn, one-shot effects still finish (AutoDestroy)
            UpdateAnimation(scaledDeltaTime, nullptr);
        }

        ProcessPendingEntityDestructions();
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
        other->mName = mName;

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
        CopyComponent<RendererSettingsComponent>(other->mRegistry, mRegistry, enttMap);
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
        CopyComponent<ParticleSystemComponent>(other->mRegistry, mRegistry, enttMap);
        CopyComponent<AnimatorComponent>(other->mRegistry, mRegistry, enttMap);
    }

    Entity Scene::FindEntityByUUID(UUID id)
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
        if(!entity || !mRegistry.valid(entity.Raw()))
            return;

        // Prevent duplicate entries in the destruction queue
        entt::entity raw = entity.Raw();
        for(entt::entity queued : mEntitiesToDestroy)
        {
            if(queued == raw)
                return;
        }
        mEntitiesToDestroy.push_back(raw);
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
        CopyIfHas(std::type_identity<ParticleSystemComponent>{});
        CopyIfHas(std::type_identity<AnimatorComponent>{});
        CopyIfHas(std::type_identity<RendererSettingsComponent>{});

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

    // A script entry by index, fetched again after every Lua call: a callback may remove the component or destroy the entity
    static ScriptEntry* FindScriptEntry(entt::registry& registry, entt::entity entity, size_t index)
    {
        if(!registry.valid(entity))
            return nullptr;
        ScriptComponent* component = registry.try_get<ScriptComponent>(entity);
        return (component && index < component->Scripts.size()) ? &component->Scripts[index] : nullptr;
    }

    static size_t ScriptCount(entt::registry& registry, entt::entity entity)
    {
        const ScriptComponent* component = registry.valid(entity) ? registry.try_get<ScriptComponent>(entity) : nullptr;
        return component ? component->Scripts.size() : 0;
    }

    void Scene::UpdateScripts()
    {
        SURGE_PROFILE_FUNC("Scene::UpdateScripts");

        if(!mIsRunning)
            return;

        ScriptEngine* scriptEngine = mContext.ScriptEngine_;
        const float unscaledDeltaTime = mContext.Clock_->GetSeconds();
        const float deltaTime = unscaledDeltaTime * mContext.Clock_->GetTimeScale();
        scriptEngine->AdvanceTime(deltaTime, unscaledDeltaTime);

        // Contacts of the fixed steps that already ran this frame, before OnUpdate like the steps themselves
        DispatchPhysicsEvents();

        // Snapshot (reused buffer): scripts may add/remove entities while they run
        auto view = mRegistry.view<ScriptComponent>();
        mScriptEntities.assign(view.begin(), view.end());

        // Scripts added from Lua (ScriptComponent:AddScript) join now, never while a list is iterated
        for(entt::entity entityID : mScriptEntities)
        {
            ScriptComponent* component = mRegistry.try_get<ScriptComponent>(entityID);
            if(!component || component->PendingScripts.empty())
                continue;
            for(Ref<Asset>& asset : component->PendingScripts)
                component->Scripts.emplace_back().ScriptAsset = std::move(asset);
            component->PendingScripts.clear();
        }

        // Instantiate new scripts, flip enabled state, OnUpdate
        mCreatedScripts.clear();
        for(entt::entity entityID : mScriptEntities)
        {
            const size_t count = ScriptCount(mRegistry, entityID);
            for(size_t i = 0; i < count; i++)
            {
                ScriptEntry* entry = FindScriptEntry(mRegistry, entityID, i);
                if(!entry)
                    break;

                const bool shouldRun = mRegistry.get<ScriptComponent>(entityID).Active && entry->Active && entry->ScriptAsset;
                if(!entry->IsInstantiated)
                {
                    if(shouldRun)
                    {
                        // Every new script gets its environment before any OnCreate runs, so OnCreate can reach the others with entity:GetScript()
                        scriptEngine->Instantiate(*entry->ScriptAsset.As<Script>(), entry->Instance, Entity(entityID, this), entry->Fields);
                        entry->IsInstantiated = true; // Also when loading failed: the error is reported once, not every frame
                        mCreatedScripts.emplace_back(entityID, static_cast<Uint>(i));
                    }
                    continue;
                }

                if(shouldRun != entry->IsEnabled)
                {
                    entry->IsEnabled = shouldRun;
                    if(shouldRun)
                        scriptEngine->CallOnEnable(entry->Instance);
                    else
                        scriptEngine->CallOnDisable(entry->Instance);

                    entry = FindScriptEntry(mRegistry, entityID, i);
                    if(!entry)
                        break;
                }

                if(entry->IsEnabled)
                    scriptEngine->CallOnUpdate(entry->Instance, deltaTime);
            }
        }

        for(const auto& [entityID, index] : mCreatedScripts)
        {
            ScriptEntry* entry = FindScriptEntry(mRegistry, entityID, index);
            if(!entry || !entry->Instance.IsValid())
                continue;

            scriptEngine->CallOnCreate(entry->Instance);
            entry = FindScriptEntry(mRegistry, entityID, index);
            if(!entry)
                continue;
            entry->IsEnabled = true;
            entry->StartedThisFrame = true;
            scriptEngine->CallOnEnable(entry->Instance);
        }

        {
            UI::Manager& uiManager = mContext.Renderer_->GetUIManager();
            auto canvasView = mRegistry.view<UICanvasComponent>();
            mScriptEntities.assign(canvasView.begin(), canvasView.end());

            for(entt::entity entityID : mScriptEntities)
            {
                if(!mRegistry.valid(entityID) || !mRegistry.all_of<UICanvasComponent>(entityID))
                    continue;

                auto& uiComp = mRegistry.get<UICanvasComponent>(entityID);
                Entity entityObj = { entityID, this };

                // Each canvas is its own entry in the UI Manager, ShowCanvas/Active only hide this canvas (used to toggle the whole UI)
                uiComp.RuntimeCanvasID = entityObj.GetComponent<IDComponent>().ID.Get();
                const Ref<UILayout> layout = uiComp.Layout ? uiComp.Layout.As<UILayout>() : nullptr;
                uiManager.SetCanvasProperties(uiComp.RuntimeCanvasID, uiComp.SortOrder, layout ? layout->Scaler : UI::CanvasScaler(), uiComp.Active && uiComp.ShowCanvas);

                if(!uiComp.Active || !uiComp.ShowCanvas)
                    continue;

                if(!uiComp.IsInstantiated)
                {
                    // Layout first, so OnCreate can already find its widgets
                    if(layout)
                        uiManager.SetCanvasLayoutRoot(uiComp.RuntimeCanvasID, layout->Instantiate());

                    if(uiComp.ScriptAsset)
                    {
                        uiManager.BeginCanvasScript(uiComp.RuntimeCanvasID);
                        if(scriptEngine->Instantiate(*uiComp.ScriptAsset.As<Script>(), uiComp.Instance, entityObj, uiComp.Fields))
                            scriptEngine->CallOnCreate(uiComp.Instance);
                        uiManager.EndCanvasScript();
                    }
                    uiComp.IsInstantiated = true;
                }
                else if(uiComp.Instance.OnUpdate.valid())
                {
                    uiManager.BeginCanvasScript(uiComp.RuntimeCanvasID);
                    scriptEngine->CallOnUpdate(uiComp.Instance, deltaTime);
                    uiManager.EndCanvasScript();
                }
            }
        }

        // OnLateUpdate: after every OnUpdate (scripts and UI) of the frame, e.g. cameras following what moved
        auto lateView = mRegistry.view<ScriptComponent>();
        mScriptEntities.assign(lateView.begin(), lateView.end());
        for(entt::entity entityID : mScriptEntities)
        {
            const size_t count = ScriptCount(mRegistry, entityID);
            for(size_t i = 0; i < count; i++)
            {
                ScriptEntry* entry = FindScriptEntry(mRegistry, entityID, i);
                if(!entry)
                    break;
                if(entry->StartedThisFrame) // OnCreate ran instead of OnUpdate this frame
                {
                    entry->StartedThisFrame = false;
                    continue;
                }
                if(entry->IsEnabled)
                    scriptEngine->CallOnLateUpdate(entry->Instance, deltaTime);
            }
        }
    }

    void Scene::FixedUpdateScripts(float fixedDeltaTime)
    {
        SURGE_PROFILE_FUNC("Scene::FixedUpdateScripts");
        if(!mIsRunning)
            return;

        ScriptEngine* scriptEngine = mContext.ScriptEngine_;
        auto view = mRegistry.view<ScriptComponent>();
        mFixedScriptEntities.assign(view.begin(), view.end());
        for(entt::entity entityID : mFixedScriptEntities)
        {
            const size_t count = ScriptCount(mRegistry, entityID);
            for(size_t i = 0; i < count; i++)
            {
                ScriptEntry* entry = FindScriptEntry(mRegistry, entityID, i);
                if(!entry)
                    break;
                if(entry->IsEnabled)
                    scriptEngine->CallOnFixedUpdate(entry->Instance, fixedDeltaTime);
            }
        }
    }

    void Scene::DispatchPhysicsEvents()
    {
        const Vector<CollisionEvent> events = mContext.Physics_->GetContactListener().FlushEvents();
        if(events.empty())
            return;

        ScriptEngine* scriptEngine = mContext.ScriptEngine_;
        auto dispatch = [this, scriptEngine](entt::entity self, entt::entity other, const CollisionEvent& event) {
            const bool enter = event.Type == CollisionEventType::ENTER;
            if(enter && !mRegistry.valid(other))
                return; // Exits still arrive when the other entity was destroyed (it is then an invalid Entity in Lua)

            const Entity otherEntity(other, this);
            const size_t count = ScriptCount(mRegistry, self);
            for(size_t i = 0; i < count; i++)
            {
                ScriptEntry* entry = FindScriptEntry(mRegistry, self, i);
                if(!entry)
                    break;
                if(!entry->IsEnabled)
                    continue;

                if(event.IsTrigger)
                    enter ? scriptEngine->CallOnTriggerEnter(entry->Instance, otherEntity) : scriptEngine->CallOnTriggerExit(entry->Instance, otherEntity);
                else
                    enter ? scriptEngine->CallOnCollisionEnter(entry->Instance, otherEntity) : scriptEngine->CallOnCollisionExit(entry->Instance, otherEntity);
            }
        };

        for(const CollisionEvent& event : events)
        {
            const entt::entity a = static_cast<entt::entity>(event.EnttIDA);
            const entt::entity b = static_cast<entt::entity>(event.EnttIDB);
            dispatch(a, b, event);
            dispatch(b, a, event); // A's callback may have destroyed B: dispatch checks again
        }
    }

    void Scene::UpdatePhysics()
    {
        SURGE_PROFILE_FUNC("Scene::UpdatePhysics");

        if(!mIsRunning)
            return;

        Physics* physics = mContext.Physics_;
        auto view = mRegistry.view<TransformComponent, RigidbodyComponent>();
        for(auto [entity, transformComp, rb] : view.each())
        {
            if(physics->IsInValid(rb.RuntimeBodyID) || !physics->IsActive(rb.RuntimeBodyID) || !rb.Active)
                continue;

            if (rb.Interpolate)
            {
                transformComp.Position = physics->GetInterpolatedPosition(rb.RuntimeBodyID);
                transformComp.Rotation = physics->GetInterpolatedRotation(rb.RuntimeBodyID);
            }
            else
            {
                transformComp.Position = physics->GetPosition(rb.RuntimeBodyID);
                transformComp.Rotation = physics->GetRotation(rb.RuntimeBodyID);
            }

            transformComp.MarkDirty();
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

        glm::vec2 windowSize = mContext.Window_->GetSize();
        OnResize(windowSize.x, windowSize.y);

        AssetManager* assetManager = mContext.AssetManager_;
        {
            Entity e;
            CreateEntity(e, "Cube");
            MeshComponent& meshComp = e.AddComponent<MeshComponent>();
            AssetID kek = assetManager->GetBuiltin(DefaultMesh::CUBE, AssetType::MESH);
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
            AssetID kek = assetManager->GetBuiltin(DefaultMesh::CYLINDER, AssetType::MESH);
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
            CreateEntity(env, "Environment");
            env.AddComponent<EnvironmentComponent>();
            env.AddComponent<RendererSettingsComponent>();
        }
    }

    void Scene::OnColliderAdded(entt::registry& registry, entt::entity entity)
    {
        if(!registry.all_of<RigidbodyComponent, TransformComponent>(entity) ||
           !(registry.any_of<BoxColliderComponent, SphereColliderComponent, CapsuleColliderComponent, CylinderColliderComponent, ConvexColliderComponent, MeshColliderComponent>(entity)))
            return;

        mContext.Physics_->CreateRigidbody(Entity(entity, this));
    }

    void Scene::OnRigidbodyDestroyed([[maybe_unused]] entt::registry& registry, entt::entity entity)
    {
        mContext.Physics_->DestroyRigidbody(Entity(entity, this));
    }

    void Scene::OnScriptDestroyed(entt::registry&, entt::entity entity)
    {
        ScriptEngine* scriptEngine = mContext.ScriptEngine_;
        const size_t count = ScriptCount(mRegistry, entity);
        for(size_t i = 0; i < count; i++)
        {
            ScriptEntry* entry = FindScriptEntry(mRegistry, entity, i);
            if(!entry)
                break;
            if(!entry->IsInstantiated)
                continue;

            if(entry->IsEnabled)
            {
                entry->IsEnabled = false;
                scriptEngine->CallOnDisable(entry->Instance);
                if(!(entry = FindScriptEntry(mRegistry, entity, i)))
                    break;
            }
            scriptEngine->CallOnDestroy(entry->Instance);
            if(!(entry = FindScriptEntry(mRegistry, entity, i)))
                break;
            entry->Instance = ScriptInstance();
            entry->IsInstantiated = false;
        }
    }
    void Scene::OnUICanvasDestroyed(entt::registry&, entt::entity entity)
    {
        Entity e(entity, this);
        UICanvasComponent& comp = e.GetComponent<UICanvasComponent>();
        UI::Manager& uiManager = mContext.Renderer_->GetUIManager();
        if(comp.IsInstantiated)
        {
            if(comp.Instance.OnDestroy.valid())
            {
                uiManager.BeginCanvasScript(comp.RuntimeCanvasID); // SetUIRoot(nil) in OnDestroy must target this canvas
                mContext.ScriptEngine_->CallOnDestroy(comp.Instance);
                uiManager.EndCanvasScript();
            }
            comp.Instance = ScriptInstance();
            comp.IsInstantiated = false;
        }

        // Without this the UI of a stopped scene stayed on screen (and its Lua callbacks stayed clickable)
        if(comp.RuntimeCanvasID != 0)
        {
            uiManager.RemoveCanvas(comp.RuntimeCanvasID);
            comp.RuntimeCanvasID = 0;
        }
    }

    void Scene::OnAudioSourceComponentDestroyed(entt::registry&, entt::entity entity)
    {
        Entity e(entity, this);
        AudioSourceComponent& comp = e.GetComponent<AudioSourceComponent>();
        if(comp.IsInitialized && comp.RuntimeID)
            mContext.AudioEngine_->DestroyAudio(comp.RuntimeID);
    }

} // namespace Surge
