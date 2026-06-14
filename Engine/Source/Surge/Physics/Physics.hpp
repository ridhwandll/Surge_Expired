// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include "Jolt/Renderer/DebugRenderer.h"


namespace JPH
{
    class PhysicsSystem;
    class JobSystemThreadPool;
    class TempAllocatorImpl;
    class DebugRenderer;
}

namespace Surge
{
    namespace PhysicsLayers
    {
        static constexpr JPH::ObjectLayer STATIC = 0;
        static constexpr JPH::ObjectLayer DYNAMIC = 1;
        static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
    }

    namespace BroadPhaseLayers
    {
        static constexpr JPH::BroadPhaseLayer STATIC(0);
        static constexpr JPH::BroadPhaseLayer DYNAMIC(1);
        static constexpr Uint NUM_LAYERS(2);
    }

    class Entity;
    class Physics
    {
    public:
        void Initialize();
        void Update(float deltaTime);
        void Shutdown();

        void OptimizeBroadPhase();

        JPH::ShapeRefC CreateShape(Entity entity);

        // Getters for the ECS to use
        JPH::PhysicsSystem* Get() { return mPhysicsSystem; }
        JPH::DebugRenderer* GetDebugRenderer() { return mDebugRenderer; }

        void GetDebugStats(int& outActiveBodies, int& outTotalBodies);
    private:
        // Core Jolt Systems
        JPH::PhysicsSystem* mPhysicsSystem;

        // Memory and Threading
        JPH::JobSystemThreadPool* mJobSystem;
        JPH::TempAllocatorImpl* mTempAllocator;

        void* mBPLayerInterface;
        void* mObjVsBPLayerFilter;
        void* mObjVsObjLayerFilter;
        JPH::DebugRenderer* mDebugRenderer;
    };

} // namespace Surge
