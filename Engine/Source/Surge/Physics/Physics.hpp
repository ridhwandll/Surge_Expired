// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Physics/RigidbodyID.hpp"
#include "Surge/Physics/PhysicsSettings.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/quaternion_float.hpp>

#include "ContactListener.hpp"
#include <functional>

namespace JPH
{
    class PhysicsSystem;
    class JobSystemThreadPool;
    class TempAllocatorImpl;
    class DebugRenderer;
}

namespace Surge
{
    class Entity;
    class Mesh;
    struct RigidbodyComponent;

    struct RaycastHit
    {
        uint64_t EnttID = 0;              // Body user data: the entt handle of the hit entity
        glm::vec3 Point { 0.0f };
        glm::vec3 Normal { 0.0f, 1.0f, 0.0f };
        float Distance = 0.0f;
    };

    struct BodyRuntimeData
    {
        glm::vec3 PreviousPosition { 0.0f };
        glm::quat PreviousRotation { 1.0f, 0.0f, 0.0f, 0.0f }; // identity, w-first
        bool Active = true;
    };

    class Physics
    {
    public:
        void Initialize();
        void Update(float deltaTime);
        void Shutdown();

        void OptimizeBroadPhase();

        // Gravity applies now, the collision matrix to the next broad phase pairs, the fixed rate to the next step
        void ApplySettings(const PhysicsSettings& settings);
        const PhysicsSettings& GetSettings() const { return mSettings; }
        float GetFixedTimestep() const { return mFixedTimestep; }

        //TODO: Remove Entity entity from here! PhysicsSystem SHOULD NOT know about the ECS
        void CreateRigidbody(Entity entity);
        void DestroyRigidbody(Entity entity);
        JPH::ShapeRefC CreateShape(Entity entity); //TODO: Remove, exposes Jolt

        // Drops the shared convex/mesh collider shapes (bodies keep the ones they use alive). Called when the runtime scene ends
        void ClearShapeCache() { mMeshShapes.clear(); }

        void AddForce(RigidBodyID rbID, const glm::vec3& force);
        void AddImpulse(RigidBodyID rbID, const glm::vec3& impulse);
        void SetLinearVelocity(RigidBodyID rbID, const glm::vec3& velocity);
        glm::vec3 GetLinearVelocity(RigidBodyID rbID);

        void AddTorque(RigidBodyID rbID, const glm::vec3& torque);
        void AddAngularImpulse(RigidBodyID rbID, const glm::vec3& impulse);
        void SetAngularVelocity(RigidBodyID rbID, const glm::vec3& velocity);
        glm::vec3 GetAngularVelocity(RigidBodyID rbID);

        void SetLayer(RigidBodyID rbID, int layer); // Keeps the body's static/moving broad phase layer

        bool IsInValid(RigidBodyID rbID) const;
        bool IsActive(RigidBodyID rbID) const;

        glm::vec3 GetPosition(RigidBodyID rbID) const;
        glm::vec3 GetRotation(RigidBodyID rbID) const; // Euler degrees

        // Position/rotation blended between the last two fixed-timestep snapshots
        glm::vec3 GetInterpolatedPosition(RigidBodyID rbID) const;
        glm::vec3 GetInterpolatedRotation(RigidBodyID rbID) const;

        // Snaps a body's transform immediately, bypassing interpolation for this frame
        void Teleport(RigidBodyID rbID, const glm::vec3& position, const glm::vec3& rotationEuler);

        // Closest body along the ray (sensors included) whose layer bit is set in layerMask. direction does not need to be normalized
        bool Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, RaycastHit& outHit, Uint layerMask = 0xFFFFFFFF) const;

        // Runs right before every fixed simulation step with the fixed timestep (scripts' OnFixedUpdate). One owner at a time,
        // Clear only removes the callback when it still belongs to that owner
        void SetFixedStepCallback(const void* owner, std::function<void(float)> callback) { mFixedStepOwner = owner; mFixedStepCallback = std::move(callback); }
        void ClearFixedStepCallback(const void* owner)
        {
            if(mFixedStepOwner == owner)
            {
                mFixedStepOwner = nullptr;
                mFixedStepCallback = nullptr;
            }
        }

        float GetInterpolationAlpha() const { return mInterpolationAlpha; }
        void GetDebugStats(int& outActiveBodies, int& outTotalBodies);

        // Paused: Update() no longer simulates, RequestStep() runs exactly one fixed tick on the next Update() (Editor pause/step)
        void SetPaused(bool paused) { mPaused = paused; mStepRequested = false; }
        bool IsPaused() const { return mPaused; }
        void RequestStep() { mStepRequested = true; }

        ContactListener& GetContactListener() { return mContactListener; }

        JPH::PhysicsSystem* Get() { return mPhysicsSystem; }
        JPH::DebugRenderer* GetDebugRenderer() { return mDebugRenderer; }
    private:
        enum class MeshShapeType : uint8_t
        {
            CONVEX_HULL,
            TRIANGLE_MESH
        };

        // Convex hulls and triangle meshes are built once per mesh (at its origin, unscaled) and shared by every collider using it:
        // each collider wraps the shared shape in its own offset/rotation and scale
        struct CachedMeshShapes
        {
            const Mesh* Source = nullptr; // A reloaded mesh (Editor re-cook) is a new object: its shapes are rebuilt
            JPH::ShapeRefC ConvexHull;
            JPH::ShapeRefC TriangleMesh;
        };

        glm::quat GetRotationQuat(RigidBodyID rbID) const;
        void Simulate(); // One fixed timestep, snapshots the previous transforms for interpolation first

        // Null when the mesh can't be turned into that shape (the caller falls back to a box)
        JPH::ShapeRefC CreateMeshColliderShape(const Mesh& mesh, MeshShapeType type, const glm::vec3& offset, const glm::vec3& rotationDegrees, const glm::vec3& scale, Uint entityID);

        JPH::PhysicsSystem* mPhysicsSystem;
        JPH::JobSystemThreadPool* mJobSystem;
        JPH::TempAllocatorImpl* mTempAllocator;
        JPH::DebugRenderer* mDebugRenderer;
        ContactListener mContactListener;
        std::function<void(float)> mFixedStepCallback;
        const void* mFixedStepOwner = nullptr;

        std::unordered_map<RigidBodyID, BodyRuntimeData> mBodies;
        std::unordered_map<uint64_t /*Mesh AssetID*/, CachedMeshShapes> mMeshShapes;

        PhysicsSettings mSettings;
        float mFixedTimestep = 1.0f / 60.0f;
        float mInterpolationAlpha = 0.0f;
        float mAccumulatedTime = 0.0f;
        bool mPaused = false;
        bool mStepRequested = false;

        void* mBPLayerInterface;
        void* mObjVsBPLayerFilter;
        void* mObjVsObjLayerFilter;
    };

} // namespace Surge