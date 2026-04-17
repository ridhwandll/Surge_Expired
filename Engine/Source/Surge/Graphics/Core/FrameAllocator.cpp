// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Core/FrameAllocator.hpp"

namespace Surge
{
    void FrameAllocator::Initialize(uint32_t frameCount, size_t arenaSize)
    {
        SG_ASSERT(frameCount > 0, "FrameAllocator: frameCount must be > 0");
        SG_ASSERT(arenaSize  > 0, "FrameAllocator: arenaSize must be > 0");

        mArenaSize = arenaSize;
        mArenas.resize(frameCount);

        for (auto& a : mArenas)
        {
            a.begin    = static_cast<uint8_t*>(std::malloc(arenaSize));
            a.current  = a.begin;
            a.capacity = arenaSize;
            SG_ASSERT(a.begin, "FrameAllocator: failed to allocate arena memory");
        }
    }

    void FrameAllocator::Shutdown()
    {
        for (auto& a : mArenas)
        {
            if (a.begin)
            {
                std::free(a.begin);
                a.begin   = nullptr;
                a.current = nullptr;
                a.capacity = 0;
            }
        }
        mArenas.clear();
        mArenaSize = 0;
    }

    void* FrameAllocator::Alloc(size_t bytes, size_t align)
    {
        SG_ASSERT(!mArenas.empty(), "FrameAllocator::Alloc called before Initialize!");
        Arena& arena = mArenas[mCurrentFrame];

        const uintptr_t raw     = reinterpret_cast<uintptr_t>(arena.current);
        const uintptr_t aligned = (raw + align - 1u) & ~(align - 1u);
        const uintptr_t end     = aligned + bytes;

        SG_ASSERT(end <= reinterpret_cast<uintptr_t>(arena.begin) + arena.capacity,
                  "FrameAllocator: arena overflow! Increase arena size.");

        arena.current = reinterpret_cast<uint8_t*>(end);
        return reinterpret_cast<void*>(aligned);
    }

    void FrameAllocator::Reset(uint32_t frameIndex)
    {
        SG_ASSERT(frameIndex < static_cast<uint32_t>(mArenas.size()),
                  "FrameAllocator::Reset – frame index out of range");
        mArenas[frameIndex].current = mArenas[frameIndex].begin;
    }

    size_t FrameAllocator::BytesUsed() const noexcept
    {
        if (mArenas.empty())
            return 0;
        const Arena& a = mArenas[mCurrentFrame];
        return static_cast<size_t>(a.current - a.begin);
    }

} // namespace Surge
