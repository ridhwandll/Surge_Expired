// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Physics.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Core/Profiler.hpp"
#include "Surge/ECS/Scene.hpp"
#include "Surge/ECS/Components/Components.hpp"

#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Graphics/HighLevel/Mesh.hpp"
#include "Surge/Asset/AssetManager.hpp"

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
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Body/BodyLock.h>


#include <Jolt/Renderer/DebugRenderer.h>
#include <cstdarg>
#include <algorithm>

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

        PhysicsDebugBatch(const JPH::DebugRenderer::Vertex* inVertices, int /*inVertexCount*/, const uint32_t* inIndices, int inIndexCount)
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

        virtual void DrawTriangle(JPH::RVec3Arg /*inV1*/, JPH::RVec3Arg /*inV2*/, JPH::RVec3Arg /*inV3*/, JPH::ColorArg /*inColor*/, ECastShadow /*inCastShadow*/) override {}
        virtual void DrawText3D(JPH::RVec3Arg /*inPosition*/, const std::string_view& /*inString*/, JPH::ColorArg /*inColor*/, float /*inHeight*/) override {}

        virtual Batch CreateTriangleBatch(const Triangle* inTriangles, int inTriangleCount) override { return new PhysicsDebugBatch(inTriangles, inTriangleCount);; }
        virtual Batch CreateTriangleBatch(const Vertex* inVertices, int inVertexCount, const uint32_t* inIndices, int inIndexCount) override { return new PhysicsDebugBatch(inVertices, inVertexCount, inIndices, inIndexCount); }
        virtual void DrawGeometry(JPH::RMat44Arg inModelMatrix, const JPH::AABox& /*inWorldSpaceBounds*/, float /*inLODScaleSq*/, JPH::ColorArg inModelColor, const GeometryRef& inGeometry, ECullMode, ECastShadow, EDrawMode) override
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

                Renderer* renderer = Core::GetRenderer();
                renderer->SubmitLine(glmV0, glmV1, color);
                renderer->SubmitLine(glmV1, glmV2, color);
                renderer->SubmitLine(glmV2, glmV0, color);
            }
        }
    };

    /*
        (Rid) Jolt object layers 
        Let, userlayer = 2

        Make(userlayer, isStatic) =  userlayer * 2 + isStatic
        isStatic = true
        userlayer * 2 + isStatic = 2 * 2 + 1 = 5; binary: 0101
        isStatic = false
        userlayer * 2 + isStatic = 2 * 2 + 0 = 4; binary: 0100
        The 1st bit (LSB) is used to determine if the object is static or dynamic, while the remaining bits represent the user-defined layer.
        This allows for a maximum of 16 user layers (0-15) and distinguishes between static and dynamic objects within those layers.

        IsStatic(objectLayer) = (objectLayer & 1) == 0
        if objectLayer is 5 (0101), IsStatic(5) = (5 & 1) == 0 => (0101 & 0001) == 0 => 0001 == 0 => false (dynamic)
        if objectLayer is 4 (0100), IsStatic(4) = (4 & 1) == 0 => (0100 & 0001) == 0 => 0000 == 0 => true (static)

        GetLayer(objectLayer) = objectLayer >> 1
        if objectLayer is 4 (0100), GetLayer(4) = 4 >> 1 => 0100 >> 1 => 0010 => 2 (userlayer)
        if objectLayer is 5 (0101), GetLayer(5) = 5 >> 1 => 0101 >> 1 => 0010 => 2 (userlayer)
    */
    namespace PhysicsLayers
    {
        static constexpr JPH::ObjectLayer NUM_LAYERS = PhysicsSettings::MAX_LAYERS * 2;

        static constexpr JPH::ObjectLayer Make(int userLayer, bool isStatic) { return static_cast<JPH::ObjectLayer>(userLayer * 2 + (isStatic ? 0 : 1)); }
        static constexpr bool IsStatic(JPH::ObjectLayer objectLayer) { return (objectLayer & 1) == 0; }
        static constexpr int GetLayer(JPH::ObjectLayer objectLayer) { return objectLayer >> 1; }
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
        virtual uint32_t GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }
        virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
        {
            JPH_ASSERT(inLayer < PhysicsLayers::NUM_LAYERS);
            return PhysicsLayers::IsStatic(inLayer) ? BroadPhaseLayers::STATIC : BroadPhaseLayers::DYNAMIC;
        }
    };

    class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
    {
    public:
        virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
        {
            return !PhysicsLayers::IsStatic(inLayer1) || inLayer2 == BroadPhaseLayers::DYNAMIC;
        }
    };

    // Reads the live settings: collision matrix edits apply to the next broad phase pairs
    class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
    {
    public:
        explicit ObjectLayerPairFilterImpl(const PhysicsSettings* settings) : mSettings(settings) {}

        virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override
        {
            if(PhysicsLayers::IsStatic(inObject1) && PhysicsLayers::IsStatic(inObject2))
                return false;

            return mSettings->ShouldCollide(PhysicsLayers::GetLayer(inObject1), PhysicsLayers::GetLayer(inObject2));
        }
    private:
        const PhysicsSettings* mSettings;
    };

    // Raycast layer mask: bit n = user layer n
    class LayerMaskFilter final : public JPH::ObjectLayerFilter
    {
    public:
        explicit LayerMaskFilter(Uint mask) : mMask(mask) {}
        virtual bool ShouldCollide(JPH::ObjectLayer inLayer) const override { return (mMask >> PhysicsLayers::GetLayer(inLayer)) & 1u; }
    private:
        Uint mMask;
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
        mObjVsObjLayerFilter = new ObjectLayerPairFilterImpl(&mSettings);

        const Uint cMaxBodyPairs = 65536;          // Default: 1024
        const Uint cMaxContactConstraints = 10240; // Default: 1024

        mPhysicsSystem->Init(
            1024, 0, cMaxBodyPairs, cMaxContactConstraints,
            *static_cast<BPLayerInterfaceImpl*>(mBPLayerInterface),
            *static_cast<ObjectVsBroadPhaseLayerFilterImpl*>(mObjVsBPLayerFilter),
            *static_cast<ObjectLayerPairFilterImpl*>(mObjVsObjLayerFilter)
        );

        mPhysicsSystem->SetContactListener(&mContactListener);
        ApplySettings(mSettings);
    }

    void Physics::ApplySettings(const PhysicsSettings& settings)
    {
        mSettings = settings;
        mSettings.FixedRate = std::clamp(mSettings.FixedRate, PhysicsSettings::MIN_FIXED_RATE, PhysicsSettings::MAX_FIXED_RATE);
        mSettings.CollisionSubsteps = std::clamp(mSettings.CollisionSubsteps, 1, PhysicsSettings::MAX_SUBSTEPS);
        mFixedTimestep = mSettings.GetFixedTimestep();
        mPhysicsSystem->SetGravity(JPH::Vec3(mSettings.Gravity.x, mSettings.Gravity.y, mSettings.Gravity.z));
    }

    void Physics::Update(float deltaTime)
    {
        if(mPaused)
        {
            if(mStepRequested)
            {
                Simulate();
                mStepRequested = false;
                mAccumulatedTime = 0.0f;
                mInterpolationAlpha = 1.0f; // Show the state that was just simulated, not the blend towards it
            }
            return;
        }

        float frameTime = std::min(deltaTime, 0.25f); // Prevent the "Spiral of Death": clamp spike delta times (e.g., loading freezes, OS pauses)
        mAccumulatedTime += frameTime;

        while(mAccumulatedTime >= mFixedTimestep)
        {
            Simulate();
            mAccumulatedTime -= mFixedTimestep;
        }

        mInterpolationAlpha = mAccumulatedTime / mFixedTimestep;
    }

    void Physics::Simulate()
    {
        if(mFixedStepCallback)
            mFixedStepCallback(mFixedTimestep); // Scripts first: forces they add apply to this step

        for(auto& [id, data] : mBodies)
        {
            if(!data.Active)
                continue;

            data.PreviousPosition = GetPosition(id);
            data.PreviousRotation = GetRotationQuat(id);
        }

        mPhysicsSystem->Update(mFixedTimestep, mSettings.CollisionSubsteps, mTempAllocator, mJobSystem);
    }

    void Physics::Shutdown()
    {
        mMeshShapes.clear(); // Shapes are Jolt objects: released before Jolt goes away
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

    bool Physics::Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, RaycastHit& outHit, Uint layerMask) const
    {
        SURGE_PROFILE_FUNC("Physics::Raycast");
        const float length = glm::length(direction);
        if(length <= 0.0f || maxDistance <= 0.0f)
            return false;

        const glm::vec3 ray = direction / length * maxDistance;
        const JPH::RRayCast rayCast(JPH::RVec3(origin.x, origin.y, origin.z), JPH::Vec3(ray.x, ray.y, ray.z));
        JPH::RayCastResult result;
        const LayerMaskFilter layerFilter(layerMask);
        if(!mPhysicsSystem->GetNarrowPhaseQuery().CastRay(rayCast, result, {}, layerFilter))
            return false;

        const JPH::RVec3 point = rayCast.GetPointOnRay(result.mFraction);
        JPH::BodyLockRead lock(mPhysicsSystem->GetBodyLockInterface(), result.mBodyID);
        if(!lock.Succeeded())
            return false;

        const JPH::Body& body = lock.GetBody();
        const JPH::Vec3 normal = body.GetWorldSpaceSurfaceNormal(result.mSubShapeID2, point);
        outHit.EnttID = body.GetUserData();
        outHit.Point = glm::vec3(static_cast<float>(point.GetX()), static_cast<float>(point.GetY()), static_cast<float>(point.GetZ()));
        outHit.Normal = glm::vec3(normal.GetX(), normal.GetY(), normal.GetZ());
        outHit.Distance = result.mFraction * maxDistance;
        return true;
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

        auto& transform = entity.GetComponent<TransformComponent>();
        auto& rb = entity.GetComponent<RigidbodyComponent>();

        JPH::EMotionType motionType = JPH::EMotionType::Dynamic;
        if(rb.Type == RigidbodyType::STATIC)
            motionType = JPH::EMotionType::Static;
        else if(rb.Type == RigidbodyType::KINEMATIC)
            motionType = JPH::EMotionType::Kinematic;

        const JPH::ObjectLayer objectLayer = PhysicsLayers::Make(PhysicsSettings::ClampLayer(rb.Layer), motionType == JPH::EMotionType::Static);

        // Jolt's body position is the shape's origin (= the entity's), it places the center of mass itself (Body::GetPosition() = COM - R * GetCenterOfMass())
        JPH::Vec3 joltPosition = JPH::Vec3(transform.Position.x, transform.Position.y, transform.Position.z);
        JPH::Quat joltRotation = GlmToJolt(transform.Rotation);
        JPH::BodyCreationSettings settings(shape, joltPosition, joltRotation, motionType, objectLayer);

        settings.mGravityFactor = rb.UseGravity ? 1.0f : 0.0f;
        settings.mIsSensor = rb.IsSensor;
        settings.mMotionQuality = rb.ContinuousCollision ? JPH::EMotionQuality::LinearCast : JPH::EMotionQuality::Discrete;
        settings.mLinearDamping = rb.LinearDamping;
        settings.mAngularDamping = rb.AngularDamping;
        settings.mFriction = rb.Friction;
        settings.mRestitution = rb.Bounciness;
        settings.mUserData = static_cast<uint64_t>(entity.Raw());

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

        BodyRuntimeData& data = mBodies[rb.RuntimeBodyID];
        data.PreviousPosition = glm::vec3(joltPosition.GetX(), joltPosition.GetY(), joltPosition.GetZ());
        data.PreviousRotation = glm::quat(joltRotation.GetW(), joltRotation.GetX(), joltRotation.GetY(), joltRotation.GetZ());
        data.Active = true;
    }

    void Physics::DestroyRigidbody(Entity entity)
    {
        auto& rb = entity.GetScene()->GetRegistry().get<RigidbodyComponent>(entity);

        if(JPH::BodyID(rb.RuntimeBodyID).IsInvalid())
            return;

        mContactListener.OnBodyRemoved(rb.RuntimeBodyID); // Its open contacts end now: OnCollisionExit / OnTriggerExit
        JPH::BodyInterface& bodyInterface = mPhysicsSystem->GetBodyInterface();
        bodyInterface.RemoveBody(JPH::BodyID(rb.RuntimeBodyID));
        bodyInterface.DestroyBody(JPH::BodyID(rb.RuntimeBodyID));

        mBodies.erase(rb.RuntimeBodyID);
        rb.RuntimeBodyID = JPH::BodyID().GetIndexAndSequenceNumber();
    }

    void Physics::SetLayer(RigidBodyID rbID, int layer)
    {
        JPH::BodyInterface& bodyInterface = mPhysicsSystem->GetBodyInterface();
        const JPH::BodyID bodyID(rbID);
        const bool isStatic = PhysicsLayers::IsStatic(bodyInterface.GetObjectLayer(bodyID));
        bodyInterface.SetObjectLayer(bodyID, PhysicsLayers::Make(PhysicsSettings::ClampLayer(layer), isStatic));
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

    glm::vec3 Physics::GetInterpolatedPosition(RigidBodyID rbID) const
    {
        auto it = mBodies.find(rbID);
        if(it == mBodies.end())
            return GetPosition(rbID); // untracked body (shouldn't normally happen)

        return glm::mix(it->second.PreviousPosition, GetPosition(rbID), mInterpolationAlpha);
    }

    glm::vec3 Physics::GetInterpolatedRotation(RigidBodyID rbID) const
    {
        auto it = mBodies.find(rbID);
        if(it == mBodies.end())
            return GetRotation(rbID); // untracked body (shouldn't normally happen)

        glm::quat current = GetRotationQuat(rbID);
        glm::quat blended = glm::slerp(it->second.PreviousRotation, current, mInterpolationAlpha);
        return glm::degrees(glm::eulerAngles(blended));
    }

    void Physics::Teleport(RigidBodyID rbID, const glm::vec3& position, const glm::vec3& rotationEuler)
    {
        JPH::BodyInterface& bodyInterface = mPhysicsSystem->GetBodyInterface();
        JPH::Vec3 joltPos(position.x, position.y, position.z);
        JPH::Quat joltRot = GlmToJolt(rotationEuler);

        bodyInterface.SetPositionAndRotation(JPH::BodyID(rbID), joltPos, joltRot, JPH::EActivation::Activate);

        auto it = mBodies.find(rbID);
        if(it != mBodies.end())
        {
            it->second.PreviousPosition = position;
            it->second.PreviousRotation = glm::quat(glm::radians(rotationEuler));
        }
    }

    // Where a convex/mesh collider sits relative to its mesh: local rotation, then offset, then the entity's scale (offset included)
    struct ColliderPlacement
    {
        glm::quat Rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        glm::vec3 Offset = glm::vec3(0.0f);
        glm::vec3 Scale = glm::vec3(1.0f);

        glm::vec3 Apply(const glm::vec3& point) const { return (Rotation * point + Offset) * Scale; }
    };

    // The mesh a convex/mesh collider is built from, null without one
    static const Mesh* GetColliderMesh(entt::registry& registry, entt::entity entity)
    {
        const MeshComponent* meshComp = registry.try_get<MeshComponent>(entity);
        if(!meshComp || !meshComp->MeshAsset)
            return nullptr;

        return meshComp->MeshAsset.As<Mesh>().Raw();
    }

    static JPH::ShapeRefC BuildConvexHull(const Mesh& mesh, const ColliderPlacement& placement, Uint entityID)
    {
        const Vector<Vertex>& meshVertices = mesh.GetVertices();
        if(meshVertices.empty())
        {
            Log<Severity::Warn>("[Physics] Entity {} mesh has 0 CPU vertices!", entityID);
            return nullptr;
        }

        // (Rid) Same space as the rendered mesh (node transform, like the MeshCollider), then the placement.
        // Points stay where the mesh is: the hull's center of mass is handled by Jolt, recentering here would move the collider off the mesh
        Vector<JPH::Vec3> joltVertices;
        joltVertices.reserve(meshVertices.size());
        for(const Submesh& submesh : mesh.GetSubmeshes())
        {
            const Uint lastVertex = std::min(submesh.BaseVertex + submesh.VertexCount, static_cast<Uint>(meshVertices.size()));
            for(Uint vIndex = submesh.BaseVertex; vIndex < lastVertex; ++vIndex)
            {
                const glm::vec3 p = placement.Apply(mesh.GetRestPosition(submesh, vIndex)); // Skinned meshes: their rest pose
                joltVertices.emplace_back(JPH::Vec3(p.x, p.y, p.z));
            }
        }

        if(joltVertices.empty())
        {
            Log<Severity::Warn>("[Physics] Entity {} mesh has no submesh vertices!", entityID);
            return nullptr;
        }

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
            Log<Severity::Warn>("[Physics] Entity {} hull too small ({:.5f}m), fallback!", entityID, hullDiagonal);
            return nullptr;
        }
        float convexRadius = std::clamp(hullDiagonal * 0.01f, 0.0001f, 0.05f);

        JPH::ConvexHullShapeSettings settings(joltVertices.data(), (int)joltVertices.size(), convexRadius);
        JPH::ShapeSettings::ShapeResult result = settings.Create();
        if(result.HasError())
        {
            Log<Severity::Error>("[Physics] Convex Hull failed: {}", result.GetError().c_str());
            return nullptr;
        }
        return result.Get();
    }

    static JPH::ShapeRefC BuildTriangleMesh(const Mesh& mesh, const ColliderPlacement& placement, Uint entityID)
    {
        const Vector<Vertex>& meshVertices = mesh.GetVertices();
        const Vector<Index>& meshIndices = mesh.GetIndices();
        const Vector<Submesh>& submeshes = mesh.GetSubmeshes();
        if(meshVertices.empty() || meshIndices.empty())
        {
            Log<Severity::Warn>("[Physics] Entity {} mesh missing CPU data!", entityID);
            return nullptr;
        }

        JPH::VertexList joltVertices;
        joltVertices.resize(meshVertices.size());
        for(const Submesh& submesh : submeshes)
        {
            for(Uint i = 0; i < submesh.VertexCount; ++i)
            {
                const Uint vIndex = submesh.BaseVertex + i;
                const glm::vec3 p = placement.Apply(mesh.GetRestPosition(submesh, vIndex)); // Submesh node transform (skinned meshes: their rest pose)
                joltVertices[vIndex] = JPH::Float3(p.x, p.y, p.z);
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
            return nullptr;
        }
        return result.Get();
    }

    JPH::ShapeRefC Physics::CreateMeshColliderShape(const Mesh& mesh, MeshShapeType type, const glm::vec3& offset, const glm::vec3& rotationDegrees, const glm::vec3& scale, Uint entityID)
    {
        const auto build = [&mesh, type, entityID](const ColliderPlacement& placement) -> JPH::ShapeRefC
        {
            return type == MeshShapeType::CONVEX_HULL ? BuildConvexHull(mesh, placement, entityID) : BuildTriangleMesh(mesh, placement, entityID);
        };

        // Shared shape: the mesh as it is, built once per mesh
        JPH::ShapeRefC shape;
        const AssetID meshID = mesh.GetID();
        if(meshID.IsValid())
        {
            CachedMeshShapes& cached = mMeshShapes[meshID.Get()];
            if(cached.Source != &mesh)
                cached = CachedMeshShapes { .Source = &mesh, .ConvexHull = nullptr, .TriangleMesh = nullptr };

            JPH::ShapeRefC& slot = (type == MeshShapeType::CONVEX_HULL) ? cached.ConvexHull : cached.TriangleMesh;
            if(!slot)
                slot = build(ColliderPlacement());

            shape = slot;
        }
        else
            shape = build(ColliderPlacement()); // Not an asset (built in code): nothing to share it by

        if(!shape)
            return nullptr;

        // Collider's placement around it
        const glm::quat rotation = glm::quat(glm::radians(rotationDegrees));
        if(rotationDegrees != glm::vec3(0.0f) || offset != glm::vec3(0.0f))
            shape = new JPH::RotatedTranslatedShape(JPH::Vec3(offset.x, offset.y, offset.z), JPH::Quat(rotation.x, rotation.y, rotation.z, rotation.w), shape);

        if(scale != glm::vec3(1.0f))
        {
            const JPH::Vec3 joltScale(scale.x, scale.y, scale.z);
            if(!shape->IsValidScale(joltScale))
                return build({ .Rotation = rotation, .Offset = offset, .Scale = scale }); // Non-uniform scale on a rotated collider would shear it (or a zero scale)

            shape = new JPH::ScaledShape(shape, joltScale);
        }
        return shape;
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
            const Mesh* mesh = GetColliderMesh(registry, entity.Raw());
            if(!mesh)
            {
                Log<Severity::Warn>("[Physics] Entity {} has a ConvexCollider but no mesh!", (Uint)entity);
                return new JPH::BoxShape(JPH::Vec3::sReplicate(0.5f));
            }

            convex->IsDirty = true;
            JPH::ShapeRefC shape = CreateMeshColliderShape(*mesh, MeshShapeType::CONVEX_HULL, convex->LocalOffset, convex->LocalRotation, absScale, (Uint)entity);
            if(!shape)
                return new JPH::BoxShape(JPH::Vec3::sReplicate(0.5f));
            return shape;
        }
        if(auto* meshCol = registry.try_get<MeshColliderComponent>(entity.Raw()))
        {
            const Mesh* mesh = GetColliderMesh(registry, entity.Raw());
            if(!mesh)
                return new JPH::BoxShape(JPH::Vec3::sReplicate(0.5f));

            JPH::ShapeRefC shape = CreateMeshColliderShape(*mesh, MeshShapeType::TRIANGLE_MESH, meshCol->LocalOffset, meshCol->LocalRotation, absScale, (Uint)entity);
            if(!shape)
                return new JPH::BoxShape(JPH::Vec3::sReplicate(0.5f));
            return shape;
        }
        // Default fallback
        Log<Severity::Fatal>("[Physics] CreateShape: THIS SHOULD NOT HAPPEN; Entity {} has no recognized collider component, using default box shape!", (Uint)entity);
        return new JPH::BoxShape(JPH::Vec3::sReplicate(0.5f));
    }

    void Physics::AddForce(RigidBodyID rbID, const glm::vec3& force)
    {
        JPH::BodyInterface& bodyInterface = mPhysicsSystem->GetBodyInterface();
        bodyInterface.AddForce(JPH::BodyID(rbID), JPH::Vec3(force.x, force.y, force.z), JPH::EActivation::Activate);
    }

    void Physics::AddImpulse(RigidBodyID rbID, const glm::vec3& impulse)
    {
        JPH::BodyInterface& bodyInterface = mPhysicsSystem->GetBodyInterface();
        bodyInterface.AddImpulse(JPH::BodyID(rbID), JPH::Vec3(impulse.x, impulse.y, impulse.z));
    }

    void Physics::SetLinearVelocity(RigidBodyID rbID, const glm::vec3& velocity)
    {
        JPH::BodyInterface& bodyInterface = mPhysicsSystem->GetBodyInterface();
        bodyInterface.SetLinearVelocity(JPH::BodyID(rbID), JPH::Vec3(velocity.x, velocity.y, velocity.z));
    }

    glm::vec3 Physics::GetLinearVelocity(RigidBodyID rbID)
    {
        JPH::BodyInterface& bodyInterface = mPhysicsSystem->GetBodyInterface();
        JPH::Vec3 velocity = bodyInterface.GetLinearVelocity(JPH::BodyID(rbID));
        return glm::vec3(velocity.GetX(), velocity.GetY(), velocity.GetZ());
    }

    void Physics::AddTorque(RigidBodyID rbID, const glm::vec3& torque)
    {
        JPH::BodyInterface& bodyInterface = mPhysicsSystem->GetBodyInterface();
        bodyInterface.AddTorque(JPH::BodyID(rbID), JPH::Vec3(torque.x, torque.y, torque.z), JPH::EActivation::Activate);
    }

    void Physics::AddAngularImpulse(RigidBodyID rbID, const glm::vec3& impulse)
    {
        JPH::BodyInterface& bodyInterface = mPhysicsSystem->GetBodyInterface();
        bodyInterface.AddAngularImpulse(JPH::BodyID(rbID), JPH::Vec3(impulse.x, impulse.y, impulse.z));
    }

    void Physics::SetAngularVelocity(RigidBodyID rbID, const glm::vec3& velocity)
    {
        JPH::BodyInterface& bodyInterface = mPhysicsSystem->GetBodyInterface();
        bodyInterface.SetAngularVelocity(JPH::BodyID(rbID), JPH::Vec3(velocity.x, velocity.y, velocity.z));
    }

    glm::vec3 Physics::GetAngularVelocity(RigidBodyID rbID)
    {
        JPH::BodyInterface& bodyInterface = mPhysicsSystem->GetBodyInterface();
        JPH::Vec3 velocity = bodyInterface.GetAngularVelocity(JPH::BodyID(rbID));
        return glm::vec3(velocity.GetX(), velocity.GetY(), velocity.GetZ());
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

    glm::quat Physics::GetRotationQuat(RigidBodyID rbID) const
    {
        const JPH::BodyInterface& bodyInterface = mPhysicsSystem->GetBodyInterface();
        JPH::Quat q = bodyInterface.GetRotation(JPH::BodyID(rbID));
        return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ());
    }

}  // namespace Surge