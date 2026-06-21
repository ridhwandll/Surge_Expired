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

namespace Surge
{
    static Entity sSelectedEntity;

    Scene::Scene()
    {
        AddStartupEntities();
    }

    Scene::~Scene()
    {
        mRegistry.clear();
        sSelectedEntity = Entity(entt::null, nullptr);
    }

    void Scene::OnRuntimeStart()
    {
        mIsRunning = true;

        for (const auto& [entity, rigidbody] : mRegistry.view<RigidbodyComponent>().each())
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
        auto view = mRegistry.view<ScriptComponent>();

        for(auto entityID : view)
            OnScriptDestroyed(Entity(entityID, this), view.get<ScriptComponent>(entityID));

        mIsRunning = false;
    }

    void Scene::Update(EditorCamera& camera)
    {
        SyncPhysics();
        UpdateScripts();

        Renderer* renderer = Core::GetRenderer();

        renderer->BeginFrame(camera);
        {
            auto view = mRegistry.view<SpriteRendererComponent, TransformComponent>();
            for(const auto& [entity, sprite, transform] : view.each())
            {
                ImageHandle textureHandle = sprite.Texture != AssetID(AssetID::INVALID) ? Core::GetAssetManager()->Load<Texture2D>(sprite.Texture)->GetRHIImage() : ImageHandle::Invalid();
                renderer->SubmitQuad(transform.GetTransform(), sprite.Color, textureHandle);
            }
        }
        {
            auto view = mRegistry.view<LightComponent, TransformComponent>();
            for(const auto& [entity, light, transform] : view.each())
            {
                Light gpuLight {};
                gpuLight.Color = light.Color;
                gpuLight.Intensity = light.Intensity;
                gpuLight.Radius = light.Radius;
                gpuLight.Falloff = light.Falloff;

                if(light.Type == LightType::DIRECTIONAL)
                {
                    glm::vec3 dirLightDir = transform.GetTransform()[2];
                    gpuLight.PositionType = glm::vec4(dirLightDir, 0.0f); // w = 0.0f for dir light

                    glm::vec3 forwardDir = glm::normalize(glm::vec3(transform.GetTransform()[2]));
                    glm::vec4 debugColor = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f);
                    renderer->SubmitDirLightDebug(transform.Position, forwardDir, debugColor);
                }
                else if(light.Type == LightType::POINT)
                    gpuLight.PositionType = glm::vec4(transform.Position, 1.0f); // w = 1.0f for point lights

                renderer->SubmitLight(gpuLight);
            }
        }
        {
            auto view = mRegistry.view<EnvironmentComponent>();
            for(const auto& [entity, env] : view.each())
            {
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
                if(meshComponent.MeshID)
                {
                    Ref<Mesh> mesh = Core::GetAssetManager()->Load<Mesh>(meshComponent.MeshID);
                    if(mesh) //Asset might be missing/corrupted, so check before submitting
                        renderer->SubmitMesh(transformComponent.GetTransform(), mesh, meshComponent.DropShadow);
                }
            }
            if(sSelectedEntity)
            {
                if(sSelectedEntity.HasComponent<MeshComponent>())
                {
                    const MeshComponent& meshComp = sSelectedEntity.GetComponent<MeshComponent>();
                    if(meshComp.MeshID)
                    {
                        Ref<Mesh> mesh = Core::GetAssetManager()->Load<Mesh>(meshComp.MeshID);
                        if(mesh) //Asset might be missing/corrupted, so check before submitting
                        {
                            const glm::mat4& transform = sSelectedEntity.GetComponent<TransformComponent>().GetTransform();
                            renderer->SubmitMeshOutline(transform, mesh);
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
            }
        }

        renderer->EndFrame();
    }

    void Scene::Update()
    {
        SURGE_PROFILE_FUNC("Scene::Update()");
        //Timer timer("Scene::Update()", true);

        SyncPhysics();
        UpdateScripts();

        Pair<RuntimeCamera*, glm::mat4> camera = GetMainCameraEntity();

        if (camera.Data1)
        {
            Renderer* renderer = Core::GetRenderer();
            renderer->BeginFrame(*camera.Data1, camera.Data2);
            {
                auto view = mRegistry.view<SpriteRendererComponent, TransformComponent>();
                for (const auto& [entity, sprite, transform] : view.each())
                {
                    ImageHandle textureHandle = sprite.Texture != AssetID(AssetID::INVALID) ? Core::GetAssetManager()->Load<Texture2D>(sprite.Texture)->GetRHIImage() : ImageHandle::Invalid();
                    renderer->SubmitQuad(transform.GetTransform(), sprite.Color, textureHandle);
                }
            }
            {
                auto view = mRegistry.view<LightComponent, TransformComponent>();
                for(const auto& [entity, light, transform] : view.each())
                {
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
                        gpuLight.PositionType = glm::vec4(transform.Position, 1.0f); // w = 1.0f for point lights

                    renderer->SubmitLight(gpuLight);
                }
            }
            {
                auto view = mRegistry.view<EnvironmentComponent>();
                for(const auto& [entity, env] : view.each())
                {
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
                    if(meshComponent.MeshID)
                    {
                        Ref<Mesh> mesh = Core::GetAssetManager()->Load<Mesh>(meshComponent.MeshID);
                        if(mesh) //Asset might be missing/corrupted, so check before submitting
                            renderer->SubmitMesh(transformComponent.GetTransform(), mesh, meshComponent.DropShadow);

                    }
                }
                if(sSelectedEntity && sSelectedEntity.HasComponent<MeshComponent>())
                {
                    const MeshComponent& meshComp = sSelectedEntity.GetComponent<MeshComponent>();
                    if(meshComp.MeshID)
                    {
                        Ref<Mesh> mesh = Core::GetAssetManager()->Load<Mesh>(meshComp.MeshID);
                        if (mesh)  //Asset might be missing/corrupted, so check before submitting
                        {
                            const glm::mat4& transform = sSelectedEntity.GetComponent<TransformComponent>().GetTransform();
                            renderer->SubmitMeshOutline(transform, mesh);
                        }
                    }
                }
            }

            //JPH::PhysicsSystem* system = Core::GetPhysics()->Get();
            //JPH::BodyManager::DrawSettings settings;
            //settings.mDrawBoundingBox = true;
            //settings.mDrawShape = true;
            //settings.mDrawShapeWireframe = false;
            //settings.mDrawSleepStats = false;
            //settings.mDrawVelocity = true;
            //system->DrawBodies(settings, Core::GetPhysics()->GetDebugRenderer());

            renderer->EndFrame();
        }
    }

    template <typename T>
    static void CopyComponent(entt::registry& dstRegistry, entt::registry& srcRegistry, const std::unordered_map<UUID, entt::entity>& enttMap)
    {
        auto components = srcRegistry.view<T>();
        for (entt::entity srcEntity : components)
        {
            entt::entity destEntity = enttMap.at(srcRegistry.get<IDComponent>(srcEntity).ID);

            auto& srcComponent = srcRegistry.get<T>(srcEntity);
            dstRegistry.emplace_or_replace<T>(destEntity, srcComponent);
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
        CopyComponent<TransformComponent>(other->mRegistry, mRegistry, enttMap);
        CopyComponent<SpriteRendererComponent>(other->mRegistry, mRegistry, enttMap);
        CopyComponent<MeshComponent>(other->mRegistry, mRegistry, enttMap);
        CopyComponent<CameraComponent>(other->mRegistry, mRegistry, enttMap);
        CopyComponent<LightComponent>(other->mRegistry, mRegistry, enttMap);
        CopyComponent<EnvironmentComponent>(other->mRegistry, mRegistry, enttMap);

        CopyComponent<RigidbodyComponent>(other->mRegistry, mRegistry, enttMap);
        CopyComponent<BoxColliderComponent>(other->mRegistry, mRegistry, enttMap);
        CopyComponent<SphereColliderComponent>(other->mRegistry, mRegistry, enttMap);
        CopyComponent<CapsuleColliderComponent>(other->mRegistry, mRegistry, enttMap);
        CopyComponent<CylinderColliderComponent>(other->mRegistry, mRegistry, enttMap);
        CopyComponent<ConvexColliderComponent>(other->mRegistry, mRegistry, enttMap);
        CopyComponent<MeshColliderComponent>(other->mRegistry, mRegistry, enttMap);

        CopyComponent<ScriptComponent>(other->mRegistry, mRegistry, enttMap);
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
    }

    void Scene::CreateEntityEmpty(Entity& outEntity, const String& name)
    {
        entt::entity e = mRegistry.create();
        outEntity = Entity(e, this);
        outEntity.AddComponent<IDComponent>();
        outEntity.AddComponent<NameComponent>(name);
    }

    void Scene::CreateEntityWithID(Entity& outEntity, const UUID& id, const String& name)
    {
        entt::entity e = mRegistry.create();
        outEntity = Entity(e, this);
        outEntity.AddComponent<IDComponent>(id);
        outEntity.AddComponent<NameComponent>(name);
        outEntity.AddComponent<TransformComponent>();
    }

    void Scene::DestroyEntity(Entity entity)
    {
        if(entity.HasComponent<ScriptComponent>())
            OnScriptDestroyed(entity, entity.GetComponent<ScriptComponent>());

        mRegistry.destroy(entity.Raw());
    }

    Entity Scene::DuplicateEntity(Entity entity)
    {
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
        CopyIfHas(std::type_identity<ScriptComponent>{});

        return newEntity;
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
        Pair<RuntimeCamera*, glm::mat4> camera = GetMainCameraEntity();
        if (camera.Data1)
            camera.Data1->SetViewportSize(width, height);
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
            if (camera.Primary)
            {
                result = {&camera.Camera, transform.GetTransform()};
                break;
            }
        }
        return result;
    }

    void Scene::UpdateScripts()
    {
        if(mIsRunning)
        {
            auto view = mRegistry.view<ScriptComponent>();
            AssetManager* assetManager = Core::GetAssetManager();

            for(auto entityID : view)
            {
                auto& scriptComp = view.get<ScriptComponent>(entityID);
                Ref<Script> script = assetManager->Load<Script>(scriptComp.ScriptAsset);
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
    }

    void Scene::SyncPhysics()
    {
        if(mIsRunning)
        {
            Physics* physics = Core::GetPhysics();
            auto view = mRegistry.view<TransformComponent, RigidbodyComponent>();
            for(auto [entity, transformComp, rb] : view.each())
            {
                if(physics->IsInValid(rb.RuntimeBodyID) || !physics->IsActive(rb.RuntimeBodyID))
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
            meshComp.MeshID = assetManager->Import(DefaultMesh::CUBE, AssetType::MESH);

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
            meshComp.MeshID = assetManager->Import(DefaultMesh::CYLINDER, AssetType::MESH);

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

    void Scene::OnScriptDestroyed(Entity e, ScriptComponent& comp)
    {
        if(comp.IsInstantiated)
        {
            Core::GetAssetManager()->Load<Script>(comp.ScriptAsset)->ExecuteOnDestroy(e);
            comp.IsInstantiated = false;
        }
    }

} // namespace Surge
