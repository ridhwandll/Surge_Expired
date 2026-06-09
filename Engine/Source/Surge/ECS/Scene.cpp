// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/ECS/Scene.hpp"
#include "Surge/ECS/Components.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Graphics/HighLevel/Mesh.hpp"
#include "Surge/Graphics/HighLevel/DefaultMeshes.hpp"
#include "Surge/Asset/AssetManager.hpp"

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
    }

    void Scene::OnRuntimeStart()
    {
        // TODO: Create physics scene here
    }

    void Scene::OnRuntimeEnd()
    {
        // Cleanup physics system here
    }

    void Scene::Update(EditorCamera& camera)
    {
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
            if(sSelectedEntity && sSelectedEntity.HasComponent<MeshComponent>())
            {
               const MeshComponent& meshComp = sSelectedEntity.GetComponent<MeshComponent>();
               if(meshComp.MeshID)
               {
                   Ref<Mesh> mesh = Core::GetAssetManager()->Load<Mesh>(meshComp.MeshID);
                   if (mesh) //Asset might be missing/corrupted, so check before submitting
                   {
                       const glm::mat4& transform = sSelectedEntity.GetComponent<TransformComponent>().GetTransform();
                       renderer->SubmitMeshOutline(transform, mesh);
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
            auto& destComponent = dstRegistry.emplace_or_replace<T>(destEntity, srcComponent);
        }
    }

    void Scene::CopyTo(Scene* other)
    {
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
        CopyComponent<MeshComponent>(other->mRegistry, mRegistry, enttMap);
        CopyComponent<CameraComponent>(other->mRegistry, mRegistry, enttMap);
        CopyComponent<LightComponent>(other->mRegistry, mRegistry, enttMap);
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

            Ref<Material> material = assetManager->Load<Mesh>(meshComp.MeshID)->GetMaterialAtIndex(0);
            material->Set<glm::vec3>("Albedo", glm::vec3(0.1f, 0.1f, 0.1f));
            material->Set<float>("Metallic", 0.1f);
            material->Set<float>("Roughness", 0.9f);
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

} // namespace Surge