// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Physics/RigidbodyID.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <glm/ext/vector_float3.hpp>

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
    class Physics
    {
    public:
        void Initialize();
        void Update(float deltaTime);
        void Shutdown();

        void OptimizeBroadPhase();

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
        glm::vec3 GetRotation(RigidBodyID rbID) const;

        void GetDebugStats(int& outActiveBodies, int& outTotalBodies);

        ContactListener& GetContactListener() { return mContactListener; }

        JPH::PhysicsSystem* Get() { return mPhysicsSystem; }
        JPH::DebugRenderer* GetDebugRenderer() { return mDebugRenderer; }
    private:
        JPH::PhysicsSystem* mPhysicsSystem;
        JPH::JobSystemThreadPool* mJobSystem;
        JPH::TempAllocatorImpl* mTempAllocator;
        JPH::DebugRenderer* mDebugRenderer;
        ContactListener mContactListener;

        float mAccumulatedTime = 0.0f;

        void* mBPLayerInterface;
        void* mObjVsBPLayerFilter;
        void* mObjVsObjLayerFilter;
    };

} // namespace Surge
