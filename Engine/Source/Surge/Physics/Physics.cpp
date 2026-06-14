// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Physics.hpp"
#include "Surge/ECS/Scene.hpp"

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include "Jolt/Physics/Collision/Shape/BoxShape.h"
#include "Jolt/Physics/Body/BodyCreationSettings.h"
#include "Jolt/Physics/Collision/Shape/SphereShape.h"
#include "Jolt/Physics/Collision/Shape/CapsuleShape.h"
#include <cstdarg>

namespace Surge
{
    class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
    {
    public:
        BPLayerInterfaceImpl()
        {
            mObjectToBroadPhase[PhysicsLayers::STATIC] = BroadPhaseLayers::STATIC;
            mObjectToBroadPhase[PhysicsLayers::DYNAMIC] = BroadPhaseLayers::DYNAMIC;
        }
        virtual uint32_t GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }
        virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
        {
            // Jolt asserts if we pass an invalid layer, so ensure it's within bounds
            JPH_ASSERT(inLayer < PhysicsLayers::NUM_LAYERS);
            return mObjectToBroadPhase[inLayer];
        }
    private:
        JPH::BroadPhaseLayer mObjectToBroadPhase[PhysicsLayers::NUM_LAYERS];
    };

    class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
    {
    public:
        virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
        {
            switch(inLayer1)
            {
                case PhysicsLayers::STATIC:  return inLayer2 == BroadPhaseLayers::DYNAMIC;
                case PhysicsLayers::DYNAMIC: return true; // Dynamics collide with everything
                default: return false;
            }
        }
    };

    class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
    {
    public:
        virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override
        {
            switch(inObject1)
            {
                case PhysicsLayers::STATIC:  return inObject2 == PhysicsLayers::DYNAMIC;
                case PhysicsLayers::DYNAMIC: return true;
                default: return false;
            }
        }
    };

    static void JoltTraceImpl(const char* inFMT, ...)
    {
        va_list list;
        va_start(list, inFMT);
        char buffer[1024];
        vsnprintf(buffer, sizeof(buffer), inFMT, list);
        va_end(list);
        Log<Severity::Info>("[Jolt] {}", buffer);
    }

    void Physics::Initialize()
    {
        JPH::Trace = JoltTraceImpl;
        JPH::RegisterDefaultAllocator();

        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();

        mTempAllocator = new JPH::TempAllocatorImpl(10 * 1024 * 1024);
        mJobSystem = new JPH::JobSystemThreadPool(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);

        mBPLayerInterface = new BPLayerInterfaceImpl();
        mObjVsBPLayerFilter = new ObjectVsBroadPhaseLayerFilterImpl();
        mObjVsObjLayerFilter = new ObjectLayerPairFilterImpl();

        mPhysicsSystem = new JPH::PhysicsSystem();

        mPhysicsSystem->Init(
            1024, 0, 1024, 1024,
            *static_cast<BPLayerInterfaceImpl*>(mBPLayerInterface),
            *static_cast<ObjectVsBroadPhaseLayerFilterImpl*>(mObjVsBPLayerFilter),
            *static_cast<ObjectLayerPairFilterImpl*>(mObjVsObjLayerFilter)
        );
    }

    void Physics::Update(float deltaTime)
    {
        if(!mPhysicsSystem)
            return;

        // TODO: This should be fixed timestep with an accumulator
        float clampedDelta = std::min(deltaTime, 1.0f / 30.0f);

        const int collisionSteps = 1;

        // Advance the physics world
        mPhysicsSystem->Update(clampedDelta, collisionSteps, mTempAllocator, mJobSystem);
    }

    void Physics::Shutdown()
    {
        delete mPhysicsSystem;

        delete static_cast<ObjectLayerPairFilterImpl*>(mObjVsObjLayerFilter);
        delete static_cast<ObjectVsBroadPhaseLayerFilterImpl*>(mObjVsBPLayerFilter);
        delete static_cast<BPLayerInterfaceImpl*>(mBPLayerInterface);

        delete mJobSystem;
        delete mTempAllocator;

        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }

    JPH::ShapeRefC Physics::CreateShape(Entity entity)
    {
        auto& registry = entity.GetScene()->GetRegistry();

        if(auto* box = registry.try_get<BoxColliderComponent>(entity.Raw()))
            return new JPH::BoxShape(JPH::Vec3(box->HalfExtents.x, box->HalfExtents.y, box->HalfExtents.z));
        if(auto* sphere = registry.try_get<SphereColliderComponent>(entity.Raw()))
            return new JPH::SphereShape(sphere->Radius);
        if(auto* capsule = registry.try_get<CapsuleColliderComponent>(entity.Raw()))
            return new JPH::CapsuleShape(capsule->Height * 0.5f, capsule->Radius);

        Log<Severity::Warn>("[Physics] CreateShape: Entity {} has no recognized collider component, using default box shape!", (Uint)entity);
        // Default fallback
        return new JPH::BoxShape(JPH::Vec3::sReplicate(0.5f));
    }

    void Physics::GetDebugStats(int& outActiveBodies, int& outTotalBodies)
    {
        JPH::PhysicsSystem* system = Get();
        outTotalBodies = system->GetNumBodies();
        outActiveBodies = system->GetNumActiveBodies(JPH::EBodyType::RigidBody);
    }

}  // namespace Surge

