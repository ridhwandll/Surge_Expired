// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Physics.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/ECS/Scene.hpp"

#include <glm/glm.hpp>

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
#include <Jolt/Renderer/DebugRenderer.h>
#include <cstdarg>

namespace Surge
{
    // Debug renderer
    class PhysicsDebugBatch : public JPH::RefTargetVirtual
    {
    public:
        virtual void AddRef() override { mRefCount++; }
        virtual void Release() override { if(--mRefCount == 0) delete this; }
        uint32_t mRefCount = 0;
        struct Triangle { JPH::Float3 v[3]; };
        Vector<Triangle> mTriangles;

        PhysicsDebugBatch(const JPH::DebugRenderer::Vertex* inVertices, int inVertexCount, const uint32_t* inIndices, int inIndexCount)
        {
            for(int i = 0; i < inIndexCount; i += 3)
            {
                Triangle t;
                t.v[0] = inVertices[inIndices[i]].mPosition;
                t.v[1] = inVertices[inIndices[i + 1]].mPosition;
                t.v[2] = inVertices[inIndices[i + 2]].mPosition;
                mTriangles.push_back(t);
            }
        }

        PhysicsDebugBatch(const JPH::DebugRenderer::Triangle* inTriangles, int inTriangleCount)
        {
            for(int i = 0; i < inTriangleCount; i++)
            {
                Triangle t;
                t.v[0] = inTriangles[i].mV[0].mPosition;
                t.v[1] = inTriangles[i].mV[1].mPosition;
                t.v[2] = inTriangles[i].mV[2].mPosition;
                mTriangles.push_back(t);
            }
        }
    };
    class PhysicsDebugRenderer final : public JPH::DebugRenderer
    {
    public:
        PhysicsDebugRenderer()
        {
            JPH::DebugRenderer::Initialize();
        }

        virtual void DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor) override
        {
            glm::vec3 from(inFrom.GetX(), inFrom.GetY(), inFrom.GetZ());
            glm::vec3 to(inTo.GetX(), inTo.GetY(), inTo.GetZ());
            Core::GetRenderer()->SubmitLine(from, to, glm::vec4(inColor.r / 255.0f, inColor.g / 255.0f, inColor.b / 255.0f, inColor.a / 255.0f));
        }

        virtual void DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, ECastShadow inCastShadow) override {}
        virtual void DrawText3D(JPH::RVec3Arg inPosition, const std::string_view& inString, JPH::ColorArg inColor, float inHeight) override {}

        virtual Batch CreateTriangleBatch(const Triangle* inTriangles, int inTriangleCount) override { return new PhysicsDebugBatch(inTriangles, inTriangleCount);; }
        virtual Batch CreateTriangleBatch(const Vertex* inVertices, int inVertexCount, const uint32_t* inIndices, int inIndexCount) override { return new PhysicsDebugBatch(inVertices, inVertexCount, inIndices, inIndexCount); }
        virtual void DrawGeometry(JPH::RMat44Arg inModelMatrix, const JPH::AABox& inWorldSpaceBounds, float inLODScaleSq, JPH::ColorArg inModelColor, const GeometryRef& inGeometry, ECullMode inCullMode, ECastShadow inCastShadow, EDrawMode inDrawMode) override
        {
            if(inGeometry->mLODs.empty())
                return;

            PhysicsDebugBatch* batch = static_cast<PhysicsDebugBatch*>(inGeometry->mLODs[0].mTriangleBatch.GetPtr());
            if(!batch)
                return;

            glm::vec4 color(inModelColor.r / 255.0f, inModelColor.g / 255.0f, inModelColor.b / 255.0f, inModelColor.a / 255.0f);
            for(const auto& tri : batch->mTriangles)
            {
                JPH::RVec3 v0 = inModelMatrix * JPH::Vec3(tri.v[0].x, tri.v[0].y, tri.v[0].z);
                JPH::RVec3 v1 = inModelMatrix * JPH::Vec3(tri.v[1].x, tri.v[1].y, tri.v[1].z);
                JPH::RVec3 v2 = inModelMatrix * JPH::Vec3(tri.v[2].x, tri.v[2].y, tri.v[2].z);

                glm::vec3 glmV0(v0.GetX(), v0.GetY(), v0.GetZ());
                glm::vec3 glmV1(v1.GetX(), v1.GetY(), v1.GetZ());
                glm::vec3 glmV2(v2.GetX(), v2.GetY(), v2.GetZ());

                Core::GetRenderer()->SubmitLine(glmV0, glmV1, color);
                Core::GetRenderer()->SubmitLine(glmV1, glmV2, color);
                Core::GetRenderer()->SubmitLine(glmV2, glmV0, color);
            }
        }
    };

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
        mDebugRenderer = new PhysicsDebugRenderer();

        mPhysicsSystem = new JPH::PhysicsSystem();

        mPhysicsSystem->Init(
            1024, 0, 1024, 1024,
            *static_cast<BPLayerInterfaceImpl*>(mBPLayerInterface),
            *static_cast<ObjectVsBroadPhaseLayerFilterImpl*>(mObjVsBPLayerFilter),
            *static_cast<ObjectLayerPairFilterImpl*>(mObjVsObjLayerFilter)
        );

        JPH::Vec3 globalGravity(0.0f, -9.81f, 0.0f);
        mPhysicsSystem->SetGravity(globalGravity);
    }

    void Physics::Update(float deltaTime)
    {
        if(!mPhysicsSystem)
            return;

        const int collisionSteps = 1;

        const float cFixedTimeStep = 1.0f / 60.0f;
        static float sAccumulatedTime = 0.0f;

        float frameDeltaTime = deltaTime;
        if(frameDeltaTime > 0.25f)
            frameDeltaTime = 0.25f;

        sAccumulatedTime += frameDeltaTime;
        while(sAccumulatedTime >= cFixedTimeStep)
        {
            mPhysicsSystem->Update(cFixedTimeStep, collisionSteps, mTempAllocator, mJobSystem);
            sAccumulatedTime -= cFixedTimeStep;
        }
    }

    void Physics::Shutdown()
    {
        delete mDebugRenderer;
        delete mPhysicsSystem;

        delete static_cast<ObjectLayerPairFilterImpl*>(mObjVsObjLayerFilter);
        delete static_cast<ObjectVsBroadPhaseLayerFilterImpl*>(mObjVsBPLayerFilter);
        delete static_cast<BPLayerInterfaceImpl*>(mBPLayerInterface);

        delete mJobSystem;
        delete mTempAllocator;

        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }

    void Physics::OptimizeBroadPhase()
    {
        mPhysicsSystem->OptimizeBroadPhase();
    }

    JPH::ShapeRefC Physics::CreateShape(Entity entity)
    {
        auto& registry = entity.GetScene()->GetRegistry();

        glm::vec3 scale = entity.GetComponent<TransformComponent>().Scale;
        glm::vec3 absScale(std::abs(scale.x), std::abs(scale.y), std::abs(scale.z));

        if(auto* box = registry.try_get<BoxColliderComponent>(entity.Raw()))
        {
            return new JPH::BoxShape(JPH::Vec3(box->HalfExtents.x * absScale.x, box->HalfExtents.y * absScale.y, box->HalfExtents.z * absScale.z));
        }
        if(auto* sphere = registry.try_get<SphereColliderComponent>(entity.Raw()))
        {
            float maxScale = std::max({ absScale.x, absScale.y, absScale.z });
            return new JPH::SphereShape(sphere->Radius * maxScale);
        }

        if(auto* capsule = registry.try_get<CapsuleColliderComponent>(entity.Raw()))
        {
            // Radius scales along X/Z plane, Height scales along Y
            float maxRadiusScale = std::max(absScale.x, absScale.z);
            float scaledRadius = capsule->Radius * maxRadiusScale;
            float scaledTotalHeight = capsule->Height * absScale.y;

            float halfHeightOfCylinder = (scaledTotalHeight * 0.5f) - scaledRadius;
            if(halfHeightOfCylinder < 0.0f)
                halfHeightOfCylinder = 0.0f;

            return new JPH::CapsuleShape(halfHeightOfCylinder, scaledRadius);
        }

        // Default fallback
        Log<Severity::Fatal>("[Physics] CreateShape: THIS SHOULD NOT HAPPEN Entity {} has no recognized collider component, using default box shape!", (Uint)entity);
        return new JPH::BoxShape(JPH::Vec3::sReplicate(0.5f));
    }

    void Physics::GetDebugStats(int& outActiveBodies, int& outTotalBodies)
    {
        JPH::PhysicsSystem* system = Get();
        outTotalBodies = system->GetNumBodies();
        outActiveBodies = system->GetNumActiveBodies(JPH::EBodyType::RigidBody);
    }

}  // namespace Surge

