// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/Vector.hpp"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <mutex>

namespace Surge
{
    struct CollisionEvent
    {
        uint64_t EnttIDA;
        uint64_t EnttIDB;
    };

    class ContactListener : public JPH::ContactListener
    {
    public:
        virtual JPH::ValidateResult OnContactValidate(const JPH::Body& inBody1, const JPH::Body& inBody2, JPH::RVec3Arg inBaseOffset, const JPH::CollideShapeResult& inCollisionResult) override;
        virtual void OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override;

        //TODO
        //virtual void OnContactPersisted(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override;
        //virtual void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override;

        // Called by the Main Thread to get the events and clear the queue
        Vector<CollisionEvent> FlushEvents();

    private:
        std::mutex mMutex;
        Vector<CollisionEvent> mCollisionQueue;
    };
}

