// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/ECS/Scene.hpp"
#include "Surge/ECS/Components.hpp"
#include "SurgeMath/Math.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Core/Core.hpp"

namespace Surge
{
    Scene::Scene(bool runtime)
    {
        mRuntime = runtime;
        // mParentProject = parentProject;
        // mMetadata.Name = name;
        // mMetadata.SceneUUID = UUID();
        // mMetadata.ScenePath = path;
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
        camera.OnUpdate();
        Renderer* renderer = Core::GetRenderer();
        renderer->BeginFrame(camera);
        //{
        //    auto group = mRegistry.group<MeshComponent>(entt::get<TransformComponent>);
        //    for (auto& entity : group)
        //    {
        //        auto [mesh, transformComponent] = group.get<MeshComponent, TransformComponent>(entity);
        //        if (mesh.Mesh)
        //        {
        //            renderer->SubmitMesh(mesh, transformComponent.GetTransform());
        //        }
        //    }
        //}
        //{
        //    auto group = mRegistry.group<PointLightComponent>(entt::get<TransformComponent>);
        //    for (auto& entity : group)
        //    {
        //        auto [pointLight, transformComponent] = group.get<PointLightComponent, TransformComponent>(entity);
        //        renderer->SubmitPointLight(pointLight, transformComponent.Position);
        //    }
        //}
        //{
        //    const auto& view = mRegistry.view<TransformComponent, DirectionalLightComponent>();
        //    for (auto& entity : view)
        //    {
        //        const auto& [transform, light] = view.get<TransformComponent, DirectionalLightComponent>(entity);
        //        renderer->SubmitDirectionalLight(light, glm::normalize(transform.GetTransform()[2]));
        //    }
        //}
        renderer->EndFrame();
    }

    void Scene::Update()
    {
        Pair<RuntimeCamera*, glm::mat4> camera = GetMainCameraEntity();

        if (camera.Data1)
        {
            Renderer* renderer = Core::GetRenderer();
            renderer->BeginFrame(*camera.Data1, camera.Data2);
            //{
            //    auto group = mRegistry.group<MeshComponent>(entt::get<TransformComponent>);
            //    for (auto& entity : group)
            //    {
            //        auto [mesh, transformComponent] = group.get<MeshComponent, TransformComponent>(entity);
            //        if (mesh.Mesh)
            //        {
            //            renderer->SubmitMesh(mesh, transformComponent.GetTransform());
            //        }
            //    }
            //}
            //{
            //    auto group = mRegistry.group<PointLightComponent>(entt::get<TransformComponent>);
            //    for (auto& entity : group)
            //    {
            //        auto [pointLight, transformComponent] = group.get<PointLightComponent, TransformComponent>(entity);
            //        renderer->SubmitPointLight(pointLight, transformComponent.Position);
            //    }
            //}
            //{
            //    const auto& view = mRegistry.view<TransformComponent, DirectionalLightComponent>();
            //    for (auto& entity : view)
            //    {
            //        const auto& [transform, light] = view.get<TransformComponent, DirectionalLightComponent>(entity);
            //        renderer->SubmitDirectionalLight(light, glm::normalize(transform.GetTransform()[2]));
            //    }
            //}
            renderer->EndFrame();
        }
    }

    template <typename T>
    static void CopyComponent(entt::registry& dstRegistry, entt::registry& srcRegistry, const HashMap<UUID, entt::entity>& enttMap)
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
        HashMap<UUID, entt::entity> enttMap;
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
        //CopyComponent<MeshComponent>(other->mRegistry, mRegistry, enttMap);
        CopyComponent<CameraComponent>(other->mRegistry, mRegistry, enttMap);
        CopyComponent<PointLightComponent>(other->mRegistry, mRegistry, enttMap);
        CopyComponent<DirectionalLightComponent>(other->mRegistry, mRegistry, enttMap);
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

} // namespace Surge