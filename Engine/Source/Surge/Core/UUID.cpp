// Copyright (c) - SurgeTechnologies - All rights reserved
#include "UUID.hpp"
#include <random>
#include <chrono>
#include <atomic>

namespace Surge
{
    static uint64_t GetSecureSeed()
    {
        static std::random_device sDevice;
        uint64_t high = sDevice();
        uint64_t low = sDevice();
        return (high << 32) | low;
    }

    static thread_local std::mt19937_64 sEngine { GetSecureSeed() };
    static thread_local std::uniform_int_distribution<uint64_t> sDistribution;

    static std::atomic<uint64_t> sCounter { 1 };

    static uint64_t HashMix(uint64_t x)
    {
        // Fast 64-bit mix (similar to splitmix64 finalizer)
        x ^= x >> 30;
        x *= 0xbf58476d1ce4e5b9ULL;
        x ^= x >> 27;
        x *= 0x94d049bb133111ebULL;
        x ^= x >> 31;
        return x;
    }

    UUID::UUID()
    {
        uint64_t rnd = sDistribution(sEngine);
        uint64_t time = static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
        uint64_t count = sCounter.fetch_add(1, std::memory_order_relaxed);

        uint64_t id = rnd;
        id ^= HashMix(time);
        id ^= HashMix(count);

        mID = HashMix(id);

        if(mID == INVALID) [[unlikely]]
            mID = HashMix(rnd | 0x1);
    }

} // namespace Surge
