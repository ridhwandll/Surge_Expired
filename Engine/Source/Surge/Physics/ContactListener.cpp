// Copyright (c) - SurgeTechnologies - All rights reserved
#include "ContactListener.hpp"

namespace Surge
{
    JPH::ValidateResult ContactListener::OnContactValidate(const JPH::Body& inBody1, const JPH::Body& inBody2, JPH::RVec3Arg inBaseOffset, const JPH::CollideShapeResult& inCollisionResult)
    {
        // Accept all collisions by default
        return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
    }

    void ContactListener::OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings)
    {
        uint64_t e1 = static_cast<uint64_t>(inBody1.GetUserData());
        uint64_t e2 = static_cast<uint64_t>(inBody2.GetUserData());

        // Jolt calls this from background threads
        std::lock_guard<std::mutex> lock(mMutex);
        mCollisionQueue.push_back({ e1, e2 });
    }

    //TODO
    //void ContactListener::OnContactPersisted(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings)
    //{
    //}
    //void ContactListener::OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair)
    //{
    //}

    Surge::Vector<Surge::CollisionEvent> ContactListener::FlushEvents()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        Surge::Vector<Surge::CollisionEvent> events = mCollisionQueue;
        mCollisionQueue.clear();
        return events;
    }

}
