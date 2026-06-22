// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/Memory.hpp"
#include "Surge/Core/UUID.hpp"
#include "Surge/Graphics/Camera/EditorCamera.hpp"
#include "Surge/Graphics/Camera/RuntimeCamera.hpp"
#include "Surge/Asset/Asset.hpp"

#include <entt.hpp>

namespace Surge
{
    class Scene;
    class Entity;
    struct IDComponent;
    struct TransformComponent;
    struct ScriptComponent;

    class Scene final : public Asset
    {
    public:
        Scene();
        ~Scene();
        SURGE_ASSET_TYPE(AssetType::SCENE);
        static Ref<Scene> Create() { return Ref<Scene>::Create(); }

        void OnRuntimeStart();
        void Update(); // Runtime Update
        void Update(EditorCamera& camera); // EditorView Update
        void OnRuntimeEnd();

        void CopyTo(Scene* other);
        Entity FindEntityByUUID(UUID id);

        // Entity manipulation
        void CreateEntity(Entity& outEntity, const String& name = "New Entity");
        void CreateEntityEmpty(Entity& outEntity, const String& name);
        void CreateEntityWithID(Entity& outEntity, const UUID& id, const String& name = "New Entity");
        void DestroyEntity(Entity entity);
        void SetParent(Entity entity, Entity newParent);
        Entity DuplicateEntity(Entity entity);

        void SetSelectedEntity(Entity entity);
        Entity GetSelectedEntity() const;

        void OnResize(float width, float height);

        void SetRunning(bool isRunning) { mIsRunning = isRunning; }
        bool IsRunning() const { return mIsRunning; }

        Entity GetEntityByName(const String& name);

        entt::registry& GetRegistry() { return mRegistry; }
        const entt::registry& GetRegistry() const { return mRegistry; }

        Pair<RuntimeCamera*, glm::mat4> GetMainCameraEntity(); // Camera - CameraTransform(view = glm::inverse(CameraTransform))
    private:
        void UpdateTransforms();
        void UpdateScripts();
        void SyncPhysics();
        void UpdateRendering(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec2& cameraNearFar);

        void AddStartupEntities();
        void OnColliderAdded(entt::registry& registry, entt::entity entity);
        void OnRigidbodyDestroyed(entt::registry& registry, entt::entity entity);
        void OnScriptDestroyed(Entity e, ScriptComponent& comp);

    private:
        bool mIsRunning = false;
        entt::registry mRegistry;
    };

    //
    // Entity
    //

    class Entity
    {
    public:
        Entity() = default;
        Entity(const entt::entity& handle, Scene* scene)
            : mEnttHandle(handle), mScene(scene) {}

        template <typename T>
        const T* TryGetComponent() const
        {
            const auto* component = mScene->GetRegistry().try_get<T>(mEnttHandle);
            return component;
        }

        template <typename T>
        const T& GetComponent() const
        {
            T& component = mScene->GetRegistry().get<T>(mEnttHandle);
            return component;
        }

        template <typename T>
        T& GetComponent()
        {
             T& component = mScene->GetRegistry().get<T>(mEnttHandle);
            return component;
        }

        template <typename T, typename... Args>
        T& AddComponent(Args&&... args)
        {
            T& component = mScene->GetRegistry().emplace<T>(mEnttHandle, std::forward<Args>(args)...);
            return component;
        }

        template <typename T>
        void RemoveComponent()
        {
            mScene->GetRegistry().remove<T>(mEnttHandle);
        }

        template <typename T>
        bool HasComponent()
        {
            return mScene->GetRegistry().any_of<T>(mEnttHandle);
        }

        entt::entity Raw()
        {
            return mEnttHandle;
        }

        Scene* GetScene() const
        {
            return mScene;
        }

        operator bool() const { return mEnttHandle != entt::null; }
        operator entt::entity() const { return mEnttHandle; }
        bool operator==(const Entity& other) const { return mEnttHandle == other.mEnttHandle && mScene == other.mScene; }
        bool operator!=(const Entity& other) const { return !(*this == other); }

    private:
        entt::entity mEnttHandle = entt::null;
        Scene* mScene = nullptr;
    };

} // namespace Surge
