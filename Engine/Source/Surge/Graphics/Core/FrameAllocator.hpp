// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include <cstdlib>
#include <new>

namespace Surge
{
    // ---------------------------------------------------------------------------
    // FrameAllocator
    //   Ring of N per-frame linear (bump-pointer) arenas.
    //   - Alloc()  is O(1) – pointer bump + alignment.
    //   - Reset()  is O(1) – pointer reset to arena base.
    //   - No individual deallocation; entire arena is discarded each frame.
    //
    //   Typical usage:
    //     gFrameAlloc.SetCurrentFrame(frameIndex);
    //     auto* data = gFrameAlloc.AllocConstruct<MyStruct>(args...);
    //     // data is valid until gFrameAlloc.Reset(frameIndex) next cycle.
    //
    //   NOT thread-safe. Intended for single-threaded render-thread use.
    // ---------------------------------------------------------------------------
    class SURGE_API FrameAllocator
    {
    public:
        FrameAllocator()  = default;
        ~FrameAllocator() { Shutdown(); }

        SURGE_DISABLE_COPY_AND_MOVE(FrameAllocator);

        // frameCount must equal the number of frames-in-flight.
        // arenaSize is the byte capacity of a single frame's arena.
        void Initialize(uint32_t frameCount, size_t arenaSize);
        void Shutdown();

        // Allocate 'bytes' with alignment 'align' from the current frame's arena.
        void* Alloc(size_t bytes, size_t align = alignof(std::max_align_t));

        // Reset the arena for the given frame index (call at the start of that frame).
        void Reset(uint32_t frameIndex);

        // Tell the allocator which frame is active (routes Alloc to the right arena).
        void SetCurrentFrame(uint32_t frameIndex) { mCurrentFrame = frameIndex; }

        size_t BytesUsed()    const noexcept;
        size_t ArenaCapacity() const noexcept { return mArenaSize; }

        // Placement-new helper – allocates + constructs T in the current frame's arena.
        template <typename T, typename... Args>
        T* AllocConstruct(Args&&... args)
        {
            void* mem = Alloc(sizeof(T), alignof(T));
            return new (mem) T(std::forward<Args>(args)...);
        }

    private:
        struct Arena
        {
            uint8_t* begin   = nullptr;
            uint8_t* current = nullptr;
            size_t   capacity = 0;
        };

        uint32_t     mCurrentFrame = 0;
        size_t       mArenaSize    = 0;
        Vector<Arena> mArenas;
    };

} // namespace Surge
