// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include <limits>

namespace Surge
{
    // ---------------------------------------------------------------------------
    // Handle<Tag>
    //   A generation-checked, type-safe 64-bit handle.
    //   'id'         : index into the owning pool's object array.
    //   'generation' : bumped on every Destroy() so stale handles are detected.
    // ---------------------------------------------------------------------------
    template <typename Tag>
    struct Handle
    {
        static constexpr uint32_t InvalidID = std::numeric_limits<uint32_t>::max();

        uint32_t id         = InvalidID;
        uint32_t generation = 0;

        bool IsValid() const noexcept { return id != InvalidID; }

        static Handle<Tag> Invalid() noexcept { return {}; }

        bool operator==(const Handle<Tag>& o) const noexcept { return id == o.id && generation == o.generation; }
        bool operator!=(const Handle<Tag>& o) const noexcept { return !(*this == o); }
    };

    // ---------------------------------------------------------------------------
    // HandlePool<T, Tag>
    //   Compact, contiguous object pool backed by a flat array.
    //   - Create() is O(1) amortised (recycles freed slots via a free-list).
    //   - Get()     is O(1) with a generation check in debug builds.
    //   - Destroy() is O(1); bumps the slot's generation to invalidate handles.
    //
    //   Zero virtual dispatch. Objects live in mObjects[], a cache-friendly
    //   contiguous array. Inspired by bgfx, LLGL, and Thor engine patterns.
    // ---------------------------------------------------------------------------
    template <typename T, typename Tag>
    class HandlePool
    {
    public:
        HandlePool()  = default;
        ~HandlePool() = default;

        SURGE_DISABLE_COPY(HandlePool);

        void Reserve(size_t n)
        {
            mObjects.reserve(n);
            mGenerations.reserve(n);
        }

        Handle<Tag> Create(T obj = T{})
        {
            if (!mFreeList.empty())
            {
                const uint32_t idx = mFreeList.back();
                mFreeList.pop_back();
                mObjects[idx] = std::move(obj);
                return Handle<Tag>{idx, mGenerations[idx]};
            }

            const uint32_t idx = static_cast<uint32_t>(mObjects.size());
            mObjects.push_back(std::move(obj));
            mGenerations.push_back(0);
            return Handle<Tag>{idx, 0};
        }

        T& Get(Handle<Tag> handle)
        {
            SG_ASSERT(IsAlive(handle), "HandlePool::Get – invalid or expired handle");
            return mObjects[handle.id];
        }

        const T& Get(Handle<Tag> handle) const
        {
            SG_ASSERT(IsAlive(handle), "HandlePool::Get – invalid or expired handle");
            return mObjects[handle.id];
        }

        bool IsAlive(Handle<Tag> handle) const noexcept
        {
            if (!handle.IsValid())
                return false;
            if (handle.id >= static_cast<uint32_t>(mGenerations.size()))
                return false;
            return mGenerations[handle.id] == handle.generation;
        }

        // Destroys the object at handle.id, bumps its generation, and recycles the slot.
        void Destroy(Handle<Tag> handle)
        {
            SG_ASSERT(IsAlive(handle), "HandlePool::Destroy – invalid or already-destroyed handle");
            mGenerations[handle.id]++;
            mObjects[handle.id] = T{}; // reset to default
            mFreeList.push_back(handle.id);
        }

        size_t Capacity() const noexcept { return mObjects.size(); }
        bool   IsEmpty()  const noexcept { return mObjects.empty(); }

    private:
        Vector<T>        mObjects;
        Vector<uint32_t> mGenerations;
        Vector<uint32_t> mFreeList;
    };

} // namespace Surge
