// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/ECS/Scene.hpp"
#include "Surge/ECS/Components.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Core/Profiler.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Graphics/HighLevel/Mesh.hpp"
#include "Surge/Graphics/HighLevel/DefaultMeshes.hpp"
#include "Surge/Asset/AssetManager.hpp"

#include "Surge/Physics/Physics.hpp"
#include "Jolt/Physics/Body/BodyInterface.h"
#include "Jolt/Physics/PhysicsSystem.h"
#include "Jolt/Physics/Body/BodyCreationSettings.h"

namespace Surge
{
    static Entity sSelectedEntity;
    static JPH::Quat GlmToJolt(const glm::vec3& v)
    {
        glm::quat q = glm::quat(glm::radians(v));
        return JPH::Quat(q.x, q.y, q.z, q.w);
    }

    Scene::Scene()
    {
        AddStartupEntities();
    }

    Scene::~Scene()
    {
        OnRuntimeEnd();

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
        mIsRunning = false;
    }

    void Scene::Update(EditorCamera& camera)
    {
        UpdatePhysics();
        Renderer* renderer = Core::GetRenderer();
        auto meshGroup = mRegistry.group<MeshComponent>(entt::get<TransformComponent>);
        Uint submitCount3D = meshGroup.size();

        renderer->BeginFrame(camera, submitCount3D);
        {
            auto view = mRegistry.view<SpriteRendererComponent, TransformComponent>();
            for(const auto& [entity, sprite, transform] : view.each())
                renderer->SubmitQuad(transform.GetTransform(), sprite.Color);
        }
        {
            auto view = mRegistry.view<LightComponent, TransformComponent>();
            for(const auto& [entity, light, transform] : view.each())
            {
                renderer->SubmitLight(light, transform.GetTransform(), transform.Position);
                if (light.Type == LightType::DIRECTIONAL)
                {
                    // Note: Assuming a Right-Handed system where forward is -Z. 
                    glm::vec3 forwardDir = glm::normalize(glm::vec3(transform.GetTransform()[2]));
                    glm::vec4 debugColor = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f);
                    renderer->SubmitDirLightDebug(transform.Position, forwardDir, debugColor);
                }
            }
        }
        {
            auto view = mRegistry.view<EnvironmentComponent>();
            for(const auto& [entity, env] : view.each())
            {
                renderer->SubmitEnvironment(env);
                break; // Only submit the first environment component we find
            }
        }
        {
            // 3D Meshes
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

                        JPH::ShapeRefC shape;
                        if(sSelectedEntity.HasComponent<ConvexColliderComponent>())
                        {
                            ConvexColliderComponent& convexComp = sSelectedEntity.GetComponent<ConvexColliderComponent>();
                            if(!convexComp.TempShape || convexComp.IsDirty)
                            {
                                convexComp.TempShape = Core::GetPhysics()->CreateShape(sSelectedEntity);
                                convexComp.IsDirty = false;
                            }
                            shape = convexComp.TempShape;
                        }
                        else
                            shape = Core::GetPhysics()->CreateShape(sSelectedEntity);

                        if(shape)
                        {
                            JPH::Vec3 comOffset = shape->GetCenterOfMass();
                            JPH::Vec3 joltPosition = JPH::Vec3(transformComp.Position.x, transformComp.Position.y, transformComp.Position.z);
                            JPH::Quat joltRotation = GlmToJolt(transformComp.Rotation);
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
        UpdatePhysics();

        Pair<RuntimeCamera*, glm::mat4> camera = GetMainCameraEntity();

        auto meshGroup = mRegistry.group<MeshComponent>(entt::get<TransformComponent>);
        Uint submitCount3D = (Uint)meshGroup.size();

        if (camera.Data1)
        {
            Renderer* renderer = Core::GetRenderer();
            renderer->BeginFrame(*camera.Data1, camera.Data2, submitCount3D);
            {
                auto view = mRegistry.view<SpriteRendererComponent, TransformComponent>();
                for (const auto& [entity, sprite, transform] : view.each())
                    renderer->SubmitQuad(transform.GetTransform(), sprite.Color);
            }
            {
                auto view = mRegistry.view<LightComponent, TransformComponent>();
                for (const auto& [entity, light, transform] : view.each())
                    renderer->SubmitLight(light, transform.GetTransform(), transform.Position);
            }
            {
                auto view = mRegistry.view<EnvironmentComponent>();
                for(const auto& [entity, env] : view.each())
                {
                    renderer->SubmitEnvironment(env);
                    break; // Only submit the first environment component we find
                }
            }
            {
                // 3D Meshes
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
        mRegistry.destroy(entity.Raw());
    }

    Entity Scene::DuplicateEntity(Entity entity)
    {
        Entity newEntity;
        CreateEntityEmpty(newEntity, entity.GetComponent<NameComponent>().Name + " (Clone)");

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

        return newEntity;
    }

    void Scene::SetSlectedEntity(Entity entity)
    {
        sSelectedEntity = entity;
    }

    Surge::Entity Scene::GetSlectedEntity() const
    {
        return sSelectedEntity;
    }

    void Scene::OnResize(float width, float height)
    {
        Pair<RuntimeCamera*, glm::mat4> camera = GetMainCameraEntity();
        if (camera.Data1)
            camera.Data1->SetViewportSize(width, height);
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

    void Scene::UpdatePhysics()
    {
        if(mIsRunning)
        {
            Physics* physics = Core::GetPhysics();
            JPH::PhysicsSystem* physicsSystem = physics->Get();
            JPH::BodyInterface& bodyInterface = physicsSystem->GetBodyInterface();

            auto view = mRegistry.view<TransformComponent, RigidbodyComponent>();
            for(auto [entity, transformComp, rb] : view.each())
            {
                if(rb.RuntimeBodyID.IsInvalid())
                    continue;

                if(!bodyInterface.IsActive(rb.RuntimeBodyID))
                    continue;

                JPH::Vec3 joltPosition = bodyInterface.GetPosition(rb.RuntimeBodyID);
                JPH::Quat joltRotation = bodyInterface.GetRotation(rb.RuntimeBodyID);

                transformComp.Position = glm::vec3(joltPosition.GetX(), joltPosition.GetY(), joltPosition.GetZ());
                transformComp.Rotation = glm::degrees(glm::eulerAngles(glm::quat(joltRotation.GetW(), joltRotation.GetX(), joltRotation.GetY(), joltRotation.GetZ())));
                transformComp.MarkDirty();
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

        Physics* physics = Core::GetPhysics();
        JPH::PhysicsSystem* physicsSystem = physics->Get();

        JPH::ShapeRefC shape = physics->CreateShape(Entity(entity, this));
        JPH::Vec3 comOffset = shape->GetCenterOfMass();

        auto& transform = registry.get<TransformComponent>(entity);
        auto& rb = registry.get<RigidbodyComponent>(entity);

        JPH::EMotionType motionType = JPH::EMotionType::Dynamic;
        JPH::ObjectLayer objectLayer = PhysicsLayers::DYNAMIC;

        if(rb.Type == RigidbodyType::STATIC)
        {
            motionType = JPH::EMotionType::Static;
            objectLayer = PhysicsLayers::STATIC;
        }
        else if(rb.Type == RigidbodyType::KINEMATIC)
        {
            motionType = JPH::EMotionType::Kinematic;
            objectLayer = PhysicsLayers::DYNAMIC;
        }

        JPH::Vec3 joltPosition = JPH::Vec3(transform.Position.x, transform.Position.y, transform.Position.z);
        JPH::Quat joltRotation = GlmToJolt(transform.Rotation);
        joltPosition = joltPosition + joltRotation * comOffset;
        JPH::BodyCreationSettings settings(shape, joltPosition, GlmToJolt(transform.Rotation), motionType, objectLayer);

        settings.mGravityFactor = rb.UseGravity ? 1.0f : 0.0f;
        settings.mIsSensor = rb.IsSensor;
        settings.mMotionQuality = rb.ContinuousCollision ? JPH::EMotionQuality::LinearCast : JPH::EMotionQuality::Discrete;
        settings.mLinearDamping = rb.LinearDamping;
        settings.mAngularDamping = rb.AngularDamping;
        settings.mFriction = rb.Friction;
        settings.mRestitution = rb.Bounciness;

        if(motionType == JPH::EMotionType::Dynamic)
        {
            settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
            settings.mMassPropertiesOverride.mMass = rb.Mass;
        }
        if(rb.FreezeRotationX || rb.FreezeRotationY || rb.FreezeRotationZ)
        {
            JPH::EAllowedDOFs allowedDOFs = JPH::EAllowedDOFs::All;
            if(rb.FreezeRotationX) allowedDOFs &= ~JPH::EAllowedDOFs::RotationX;
            if(rb.FreezeRotationY) allowedDOFs &= ~JPH::EAllowedDOFs::RotationY;
            if(rb.FreezeRotationZ) allowedDOFs &= ~JPH::EAllowedDOFs::RotationZ;

            settings.mAllowedDOFs = allowedDOFs;
        }

        JPH::BodyInterface& bodyInterface = physicsSystem->GetBodyInterface();
        JPH::Body* body = bodyInterface.CreateBody(settings);

        rb.RuntimeBodyID = body->GetID();
        bodyInterface.AddBody(rb.RuntimeBodyID, JPH::EActivation::Activate);
    }

    void Scene::OnRigidbodyDestroyed(entt::registry& registry, entt::entity entity)
    {
        auto& rb = registry.get<RigidbodyComponent>(entity);

        if(rb.RuntimeBodyID.IsInvalid())
            return;

        Physics* physics = Core::GetPhysics();
        JPH::PhysicsSystem* physicsSystem = physics->Get();

        JPH::BodyInterface& bodyInterface = physicsSystem->GetBodyInterface();
        bodyInterface.RemoveBody(rb.RuntimeBodyID);
        bodyInterface.DestroyBody(rb.RuntimeBodyID);

        rb.RuntimeBodyID = JPH::BodyID();
    }

} // namespace Surge