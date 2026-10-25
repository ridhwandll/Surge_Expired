// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/Memory.hpp"
#include "Surge/Core/UUID.hpp"
#include "Surge/Core/Vector.hpp"
#include "Surge/Graphics/Camera/EditorCamera.hpp"
#include "Surge/Graphics/Camera/RuntimeCamera.hpp"
#include "Surge/Asset/Asset.hpp"
#include "Surge/ECS/SceneContext.hpp"

#include <entt.hpp>
#include <functional>

namespace Surge
{
    class Scene;
    class Entity;
    struct IDComponent;
    struct TransformComponent;
    struct ScriptComponent;
    struct UICanvasComponent;
    struct Frustum;

    class Scene final : public Asset
    {
    public:
        Scene(const String& name, const SceneContext& context);
        ~Scene();
        SURGE_ASSET_TYPE(AssetType::SCENE);
        static Ref<Scene> Create(const String& name, const SceneContext& context) { return Ref<Scene>::Create(name, context); }

        const String& GetName() { return mName; }
        const SceneContext& GetContext() const { return mContext; } // The engine systems this scene drives
        void OnRuntimeStart();
        void Update(); // Runtime Update
        // EditorView Update, editorDraw runs after the scene was submitted and before the frame ends (editor-only debug geometry)
        void Update(EditorCamera& camera, const std::function<void(Scene&)>& editorDraw = nullptr);
        void OnRuntimeEnd();

        void CopyTo(Scene* other);
        Entity FindEntityByUUID(UUID id);

        // Entity manipulation
        void CreateEntity(Entity& outEntity, const String& name = "New Entity");
        void CreateEntityWithID(Entity& outEntity, const UUID& id, const String& name = "New Entity");
        void DestroyEntity(Entity entity);
        void DestroyEntityImmediate(Entity entity);
        void SetParent(Entity entity, Entity newParent);
        Entity DuplicateEntity(Entity entity);

        void OnResize(float width, float height);

        void SetRunning(bool isRunning) { mIsRunning = isRunning; }
        bool IsRunning() const { return mIsRunning; }

        // A paused scene keeps rendering but skips audio/physics sync/scripts, Step() simulates that many frames while paused (Editor)
        void SetPaused(bool paused) { mIsPaused = paused; mStepFrames = 0; }
        bool IsPaused() const { return mIsPaused; }
        void Step(Uint frames = 1) { mStepFrames = frames; }

        Entity GetEntityByName(const String& name);

        entt::registry& GetRegistry() { return mRegistry; }
        const entt::registry& GetRegistry() const { return mRegistry; }

        Pair<RuntimeCamera*, glm::mat4> GetMainCameraEntity(); // Camera - CameraTransform(view = glm::inverse(CameraTransform))
    private:
        void CreateEntityEmpty(Entity& outEntity, const String& name); // Without TransformComponent

        void UpdateTransforms();
        float UpdateSimulation(); // Audio, physics sync and scripts, honors the pause/step state. Returns the time it advanced (particles)
        void UpdateScripts();
        void FixedUpdateScripts(float fixedDeltaTime); // Physics fixed step callback while running
        void DispatchPhysicsEvents();                  // OnCollisionEnter/Exit, OnTriggerEnter/Exit
        void UpdatePhysics();
        void UpdateAudio();
        void UpdateRendering(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec2& cameraNearFar, float scaledDeltaTime,
                             bool cull, const std::function<void(Scene&)>* editorDraw = nullptr);
        // Simulates every ParticleSystemComponent, submits the visible ones (right after Renderer::BeginFrame). frustum = null: no culling
        void UpdateParticles(float deltaTime, const Frustum* frustum, bool submit);
        // Advances + evaluates every AnimatorComponent, the mesh loop submits their poses. frustum = null: no culling
        void UpdateAnimation(float deltaTime, const Frustum* frustum);
        void ProcessPendingEntityDestructions();

    private:
        void AddStartupEntities();
        void OnColliderAdded(entt::registry& registry, entt::entity entity);

        void OnRigidbodyDestroyed(entt::registry& registry, entt::entity entity);
        void OnScriptDestroyed(entt::registry& registry, entt::entity entity);
        void OnUICanvasDestroyed(entt::registry& registry, entt::entity entity);
        void OnAudioSourceComponentDestroyed(entt::registry& registry, entt::entity entity);

    private:
        String mName;
        SceneContext mContext;
        bool mIsRunning = false;
        bool mIsPaused = false;
        Uint mStepFrames = 0;
        entt::registry mRegistry;
        Vector<entt::entity> mEntitiesToDestroy;
        Vector<entt::entity> mScriptEntities;      // UpdateScripts() scratch, reused every frame
        Vector<entt::entity> mFixedScriptEntities; // FixedUpdateScripts() scratch
        Vector<std::pair<entt::entity, Uint>> mCreatedScripts; // (entity, script index) instantiated this frame
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

        operator bool() const
        {
            return mEnttHandle != entt::null && mScene != nullptr && mScene->GetRegistry().valid(mEnttHandle);
        }
        operator entt::entity() const { return mEnttHandle; }
        bool operator==(const Entity& other) const { return mEnttHandle == other.mEnttHandle && mScene == other.mScene; }
        bool operator!=(const Entity& other) const { return !(*this == other); }

    private:
        entt::entity mEnttHandle = entt::null;
        Scene* mScene = nullptr;
    };

} // namespace Surge
