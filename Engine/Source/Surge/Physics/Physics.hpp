// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Physics/RigidbodyID.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/quaternion_float.hpp>

#include "ContactListener.hpp"

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
    struct RigidbodyComponent;

    struct BodyRuntimeData
    {
        glm::vec3 PreviousPosition { 0.0f };
        glm::quat PreviousRotation { 1.0f, 0.0f, 0.0f, 0.0f }; // identity, w-first
        bool Active = true;
    };

    class Physics
    {
    public:
        static constexpr float FIXED_TIMESTEP = 1.0f / 60.0f;
        static constexpr int COLLISION_SUBSTEPS = 2;
    public:
        void Initialize();
        void Update(float deltaTime);
        void Shutdown();

        void OptimizeBroadPhase();

        //TODO: Remove Entity entity from here! PhysicsSystem SHOULD NOT know about the ECS
        void CreateRigidbody(Entity entity);
        void DestroyRigidbody(Entity entity);
        JPH::ShapeRefC CreateShape(Entity entity); //TODO: Remove, exposes Jolt

        void AddForce(RigidBodyID rbID, const glm::vec3& force);
        void AddImpulse(RigidBodyID rbID, const glm::vec3& impulse);
        void SetLinearVelocity(RigidBodyID rbID, const glm::vec3& velocity);
        glm::vec3 GetLinearVelocity(RigidBodyID rbID);

        void AddTorque(RigidBodyID rbID, const glm::vec3& torque);
        void AddAngularImpulse(RigidBodyID rbID, const glm::vec3& impulse);
        void SetAngularVelocity(RigidBodyID rbID, const glm::vec3& velocity);
        glm::vec3 GetAngularVelocity(RigidBodyID rbID);

        bool IsInValid(RigidBodyID rbID) const;
        bool IsActive(RigidBodyID rbID) const;

        glm::vec3 GetPosition(RigidBodyID rbID) const;
        glm::vec3 GetRotation(RigidBodyID rbID) const; // Euler degrees

        // Position/rotation blended between the last two fixed-timestep snapshots
        glm::vec3 GetInterpolatedPosition(RigidBodyID rbID) const;
        glm::vec3 GetInterpolatedRotation(RigidBodyID rbID) const;

        // Snaps a body's transform immediately, bypassing interpolation for this frame
        void Teleport(RigidBodyID rbID, const glm::vec3& position, const glm::vec3& rotationEuler);

        float GetInterpolationAlpha() const { return mInterpolationAlpha; }
        void GetDebugStats(int& outActiveBodies, int& outTotalBodies);

        ContactListener& GetContactListener() { return mContactListener; }

        JPH::PhysicsSystem* Get() { return mPhysicsSystem; }
        JPH::DebugRenderer* GetDebugRenderer() { return mDebugRenderer; }
    private:
        glm::quat GetRotationQuat(RigidBodyID rbID) const;

        JPH::PhysicsSystem* mPhysicsSystem;
        JPH::JobSystemThreadPool* mJobSystem;
        JPH::TempAllocatorImpl* mTempAllocator;
        JPH::DebugRenderer* mDebugRenderer;
        ContactListener mContactListener;

        std::unordered_map<RigidBodyID, BodyRuntimeData> mBodies;

        float mInterpolationAlpha = 0.0f;
        float mAccumulatedTime = 0.0f;

        void* mBPLayerInterface;
        void* mObjVsBPLayerFilter;
        void* mObjVsObjLayerFilter;
    };

} // namespace Surge