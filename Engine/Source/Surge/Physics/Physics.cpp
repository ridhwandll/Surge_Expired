// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Physics.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/ECS/Scene.hpp"
#include "Surge/ECS/Components.hpp"

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>


#include <Jolt/Renderer/DebugRenderer.h>
#include <cstdarg>

namespace Surge
{
    static void JoltTraceCallback(const char* inFMT, ...)
    {
        va_list list;
        va_start(list, inFMT);
        char buffer[1024];
        vsnprintf(buffer, sizeof(buffer), inFMT, list);
        va_end(list);
        Log<Severity::Trace>("[Jolt] {}", buffer);
    }

#ifdef JPH_ENABLE_ASSERTS
    // Asset Callback
    static bool JoltAssertFailedCallback(const char* inExpression, const char* inMessage, const char* inFile, JPH::uint inLine)
    {
        Log<Severity::Error>("[Jolt] Assert Failed: {}:{} ({}) {}", inFile, inLine, inExpression, (inMessage ? inMessage : ""));
        return true;
    }
#endif

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
        static constexpr Uint NUM_LAYERS = 2;
    }

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

    void Physics::Initialize()
    {
        JPH::RegisterDefaultAllocator();

        JPH::Trace = JoltTraceCallback;
#ifdef JPH_ENABLE_ASSERTS
        JPH::AssertFailed = JoltAssertFailedCallback;
#endif

        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();

        mTempAllocator = new JPH::TempAllocatorImpl(20 * 1024 * 1024); //20MB buffer just in case
        mJobSystem = new JPH::JobSystemThreadPool(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);
        mDebugRenderer = new PhysicsDebugRenderer();
        mPhysicsSystem = new JPH::PhysicsSystem();

        mBPLayerInterface = new BPLayerInterfaceImpl();
        mObjVsBPLayerFilter = new ObjectVsBroadPhaseLayerFilterImpl();
        mObjVsObjLayerFilter = new ObjectLayerPairFilterImpl();

        const Uint cMaxBodyPairs = 65536;          // Default: 1024
        const Uint cMaxContactConstraints = 10240; // Default: 1024

        mPhysicsSystem->Init(
            1024, 0, cMaxBodyPairs, cMaxContactConstraints,
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

        float frameDeltaTime = deltaTime;
        if(frameDeltaTime > 0.25f)
            frameDeltaTime = 0.25f;

        mAccumulatedTime += frameDeltaTime;
        while(mAccumulatedTime >= cFixedTimeStep)
        {
            mPhysicsSystem->Update(cFixedTimeStep, collisionSteps, mTempAllocator, mJobSystem);
            mAccumulatedTime -= cFixedTimeStep;
        }
    }

    void Physics::Shutdown()
    {
        delete mPhysicsSystem;
        delete mDebugRenderer;
        delete mJobSystem;
        delete mTempAllocator;

        delete static_cast<ObjectLayerPairFilterImpl*>(mObjVsObjLayerFilter);
        delete static_cast<ObjectVsBroadPhaseLayerFilterImpl*>(mObjVsBPLayerFilter);
        delete static_cast<BPLayerInterfaceImpl*>(mBPLayerInterface);

        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }

    void Physics::OptimizeBroadPhase()
    {
        mPhysicsSystem->OptimizeBroadPhase();
    }

    static JPH::Quat GlmToJolt(const glm::vec3& v)
    {
        glm::quat q = glm::quat(glm::radians(v));
        return JPH::Quat(q.x, q.y, q.z, q.w);
    }

    void Physics::CreateRigidbody(Entity entity)
    {
        JPH::ShapeRefC shape = CreateShape(entity);
        JPH::Vec3 comOffset = shape->GetCenterOfMass();

        auto& transform = entity.GetComponent<TransformComponent>();
        auto& rb = entity.GetComponent<RigidbodyComponent>();

        JPH::EMotionType motionType = JPH::EMotionType::Dynamic;
        JPH::ObjectLayer objectLayer = PhysicsLayers::DYNAMIC;

        if(rb.Type == RigidbodyType::STATIC)
        {
            motionType = JPH::EMotionType::Static;
            objectLayer = PhysicsLayers::STATIC;
        }
        else if(rb.Type == RigidbodyType::KINEMATIC)
        {
            motionType = JPH::EMotionType::Kinematic;
            objectLayer = PhysicsLayers::DYNAMIC;
        }

        JPH::Vec3 joltPosition = JPH::Vec3(transform.Position.x, transform.Position.y, transform.Position.z);
        JPH::Quat joltRotation = GlmToJolt(transform.Rotation);
        joltPosition = joltPosition + joltRotation * comOffset;
        JPH::BodyCreationSettings settings(shape, joltPosition, GlmToJolt(transform.Rotation), motionType, objectLayer);

        settings.mGravityFactor = rb.UseGravity ? 1.0f : 0.0f;
        settings.mIsSensor = rb.IsSensor;
        settings.mMotionQuality = rb.ContinuousCollision ? JPH::EMotionQuality::LinearCast : JPH::EMotionQuality::Discrete;
        settings.mLinearDamping = rb.LinearDamping;
        settings.mAngularDamping = rb.AngularDamping;
        settings.mFriction = rb.Friction;
        settings.mRestitution = rb.Bounciness;

        if(motionType == JPH::EMotionType::Dynamic)
        {
            settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
            settings.mMassPropertiesOverride.mMass = rb.Mass;
        }
        if(rb.FreezeRotationX || rb.FreezeRotationY || rb.FreezeRotationZ)
        {
            JPH::EAllowedDOFs allowedDOFs = JPH::EAllowedDOFs::All;
            if(rb.FreezeRotationX) allowedDOFs &= ~JPH::EAllowedDOFs::RotationX;
            if(rb.FreezeRotationY) allowedDOFs &= ~JPH::EAllowedDOFs::RotationY;
            if(rb.FreezeRotationZ) allowedDOFs &= ~JPH::EAllowedDOFs::RotationZ;

            settings.mAllowedDOFs = allowedDOFs;
        }

        JPH::BodyInterface& bodyInterface = mPhysicsSystem->GetBodyInterface();
        JPH::Body* body = bodyInterface.CreateBody(settings);

        rb.RuntimeBodyID = body->GetID().GetIndexAndSequenceNumber();
        bodyInterface.AddBody(JPH::BodyID(rb.RuntimeBodyID), JPH::EActivation::Activate);
    }

    void Physics::DestroyRigidbody(Entity entity)
    {
        auto& rb = entity.GetScene()->GetRegistry().get<RigidbodyComponent>(entity);

        if(JPH::BodyID(rb.RuntimeBodyID).IsInvalid())
            return;

        JPH::BodyInterface& bodyInterface = mPhysicsSystem->GetBodyInterface();
        bodyInterface.RemoveBody(JPH::BodyID(rb.RuntimeBodyID));
        bodyInterface.DestroyBody(JPH::BodyID(rb.RuntimeBodyID));

        rb.RuntimeBodyID = JPH::BodyID().GetIndexAndSequenceNumber();
    }

    bool Physics::IsActive(RigidBodyID rbID) const
    {
        const JPH::BodyInterface& bodyInterface = mPhysicsSystem->GetBodyInterface();
        return bodyInterface.IsActive((JPH::BodyID)rbID);
    }

    glm::vec3 Physics::GetPosition(RigidBodyID rbID) const
    {
        const JPH::BodyInterface& bodyInterface = mPhysicsSystem->GetBodyInterface();
        JPH::Vec3 joltPosition = bodyInterface.GetPosition(JPH::BodyID(rbID));
        return { joltPosition.GetX(), joltPosition.GetY(), joltPosition.GetZ() };
    }

    glm::vec3 Physics::GetRotation(RigidBodyID rbID) const
    {
        const JPH::BodyInterface& bodyInterface = mPhysicsSystem->GetBodyInterface();
        JPH::Quat joltRotation = bodyInterface.GetRotation(JPH::BodyID(rbID));
        return glm::degrees(glm::eulerAngles(glm::quat(joltRotation.GetW(), joltRotation.GetX(), joltRotation.GetY(), joltRotation.GetZ())));
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
                halfHeightOfCylinder = 1.0f;

            return new JPH::CapsuleShape(halfHeightOfCylinder, scaledRadius);
        }
        if(auto* cylinder = registry.try_get<CylinderColliderComponent>(entity.Raw()))
        {
            // Radius scales along X/Z plane, Height scales along Y
            float maxRadiusScale = std::max(absScale.x, absScale.z);
            float scaledRadius = cylinder->Radius * maxRadiusScale;
            float scaledHalfHeight = cylinder->Height * 0.5f * absScale.y;

            if(scaledHalfHeight < 0.0f)
                scaledHalfHeight = 0.0f;
            if(scaledRadius < 0.0f)
                scaledRadius = 0.0f;

            return new JPH::CylinderShape(scaledHalfHeight, scaledRadius);
        }
        if(auto* convex = registry.try_get<ConvexColliderComponent>(entity.Raw()))
        {
            if(!registry.any_of<MeshComponent>(entity.Raw()))
            {
                Log<Severity::Warn>("[Physics] Entity {} has ConvexCollider but no MeshComponent!", (Uint)entity);
                return new JPH::BoxShape(JPH::Vec3::sReplicate(0.5f));
            }
            auto& meshComp = registry.get<MeshComponent>(entity.Raw());
            if(!meshComp.MeshID)
                return new JPH::BoxShape(JPH::Vec3::sReplicate(0.5f));

            Ref<Mesh> mesh = Core::GetAssetManager()->Load<Mesh>(meshComp.MeshID);
            if(!mesh)
                return new JPH::BoxShape(JPH::Vec3::sReplicate(0.5f));

            const Vector<Vertex>& meshVertices = mesh->GetVertices();
            if(meshVertices.empty())
            {
                Log<Severity::Warn>("[Physics] Entity {} mesh has 0 CPU vertices!", (Uint)entity);
                return new JPH::BoxShape(JPH::Vec3::sReplicate(0.5f));
            }

            // Rotate + Scale
            glm::quat localRot = glm::quat(glm::radians(convex->LocalRotation));
            Vector<JPH::Vec3> joltVertices;
            joltVertices.reserve(meshVertices.size());
            for(const auto& v : meshVertices)
            {
                glm::vec3 r = localRot * v.Position;
                joltVertices.emplace_back(JPH::Vec3(r.x * absScale.x, r.y * absScale.y, r.z * absScale.z));
            }

            // Subtract vertex centroid so Jolt's COM lands near origin
            JPH::Vec3 centroid = JPH::Vec3::sZero();
            for(const auto& v : joltVertices)
                centroid += v;
            centroid /= (float)joltVertices.size();
            for(auto& v : joltVertices)
                v -= centroid;

            // LocalOffset in scaled space (AFTER scale, not before)
            JPH::Vec3 scaledOffset(
                convex->LocalOffset.x * absScale.x,
                convex->LocalOffset.y * absScale.y,
                convex->LocalOffset.z * absScale.z
            );
            for(auto& v : joltVertices)
                v += scaledOffset;

            // Compute hull bounds for adaptive convex radius
            JPH::Vec3 bboxMin = joltVertices[0], bboxMax = joltVertices[0];
            for(const auto& v : joltVertices)
            {
                bboxMin = JPH::Vec3::sMin(bboxMin, v);
                bboxMax = JPH::Vec3::sMax(bboxMax, v);
            }
            float hullDiagonal = (bboxMax - bboxMin).Length();
            if(hullDiagonal < 1e-4f)
            {
                Log<Severity::Warn>("[Physics] Entity {} hull too small ({:.5f}m), fallback!", (Uint)entity, hullDiagonal);
                return new JPH::BoxShape(JPH::Vec3::sReplicate(0.5f));
            }
            float convexRadius = std::clamp(hullDiagonal * 0.01f, 0.0001f, 0.05f);

            JPH::ConvexHullShapeSettings settings(joltVertices.data(), (int)joltVertices.size(), convexRadius);
            JPH::ShapeSettings::ShapeResult result = settings.Create();

            if(result.HasError())
            {
                Log<Severity::Error>("[Physics] Convex Hull failed: {}", result.GetError().c_str());
                return new JPH::BoxShape(JPH::Vec3::sReplicate(0.5f));
            }

            convex->IsDirty = true;
            return result.Get();
        }
        if(auto* meshCol = registry.try_get<MeshColliderComponent>(entity.Raw()))
        {
            if(!registry.any_of<MeshComponent>(entity.Raw()))
                return new JPH::BoxShape(JPH::Vec3::sReplicate(0.5f));
            auto& meshComp = registry.get<MeshComponent>(entity.Raw());
            if(!meshComp.MeshID)
                return new JPH::BoxShape(JPH::Vec3::sReplicate(0.5f));

            Ref<Mesh> mesh = Core::GetAssetManager()->Load<Mesh>(meshComp.MeshID);
            if(!mesh)
                return new JPH::BoxShape(JPH::Vec3::sReplicate(0.5f));

            const Vector<Vertex>& meshVertices = mesh->GetVertices();
            const Vector<Index>& meshIndices = mesh->GetIndices();
            const Vector<Submesh>& submeshes = mesh->GetSubmeshes();
            if(meshVertices.empty() || meshIndices.empty())
            {
                Log<Severity::Warn>("[Physics] Entity {} mesh missing CPU data!", (Uint)entity);
                return new JPH::BoxShape(JPH::Vec3::sReplicate(0.5f));
            }

            // Rotate + Scale
            glm::quat localRot = glm::quat(glm::radians(meshCol->LocalRotation));
            JPH::VertexList joltVertices;
            joltVertices.resize(meshVertices.size());
            for(const Submesh& submesh : submeshes)
            {
                for(Uint i = 0; i < submesh.VertexCount; ++i)
                {
                    Uint vIndex = submesh.BaseVertex + i;
                    const Vertex& v = meshVertices[vIndex];

                    // Apply the Submesh Node Transform 
                    glm::vec4 nodePos = submesh.Transform * glm::vec4(v.Position, 1.0f);

                    // Apply Local Rotation
                    glm::vec3 r = localRot * glm::vec3(nodePos);

                    // Apply Entity Scale and Local Offset
                    joltVertices[vIndex] = JPH::Float3(
                        (r.x * absScale.x) + (meshCol->LocalOffset.x * absScale.x),
                        (r.y * absScale.y) + (meshCol->LocalOffset.y * absScale.y),
                        (r.z * absScale.z) + (meshCol->LocalOffset.z * absScale.z)
                    );
                }
            }

            JPH::IndexedTriangleList joltTriangles;
            joltTriangles.reserve(meshIndices.size());
            for(const auto& submesh : submeshes)
            {
                Uint startTriangle = submesh.BaseIndex / 3;
                Uint triangleCount = submesh.IndexCount / 3;

                for(Uint i = 0; i < triangleCount; ++i)
                {
                    const Index& idx = meshIndices[startTriangle + i];
                    joltTriangles.emplace_back(JPH::IndexedTriangle(
                        idx.V1 + submesh.BaseVertex,
                        idx.V2 + submesh.BaseVertex,
                        idx.V3 + submesh.BaseVertex,
                        0 // Material Index
                    ));
                }
            }

            JPH::MeshShapeSettings settings(joltVertices, joltTriangles);
            JPH::ShapeSettings::ShapeResult result = settings.Create();
            if(result.HasError())
            {
                Log<Severity::Error>("[Physics] MeshShape failed: {}", result.GetError().c_str());
                return new JPH::BoxShape(JPH::Vec3::sReplicate(0.5f));
            }

            return result.Get();
        }
        // Default fallback
        Log<Severity::Fatal>("[Physics] CreateShape: THIS SHOULD NOT HAPPEN; Entity {} has no recognized collider component, using default box shape!", (Uint)entity);
        return new JPH::BoxShape(JPH::Vec3::sReplicate(0.5f));
    }

    bool Physics::IsInValid(RigidBodyID rbID) const
    {
        return JPH::BodyID(rbID).IsInvalid();
    }

    void Physics::GetDebugStats(int& outActiveBodies, int& outTotalBodies)
    {
        outTotalBodies = mPhysicsSystem->GetNumBodies();
        outActiveBodies = mPhysicsSystem->GetNumActiveBodies(JPH::EBodyType::RigidBody);
    }

}  // namespace Surge
