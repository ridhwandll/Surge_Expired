// Copyright (c) - SurgeTechnologies - All rights reserved
#include "UUID.hpp"
#include <random>
#include <chrono>
#include <atomic>

namespace Surge
{
    // High-quality random engine (thread-local for performance)
    static thread_local std::mt19937_64 sEngine {std::random_device {}()};

    static std::uniform_int_distribution<uint64_t> sDistribution;

    // Atomic counter for extra uniqueness guarantees
    static std::atomic<uint64_t> sCounter {0};

    static inline uint64_t GetTimeEntropy()
    {
        return static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    }

    static inline uint64_t HashMix(uint64_t x)
    {
        // Fast 64-bit mix (similar to splitmix64 finalizer)
        // Fixed magic constant
        x ^= x >> 30;
        x *= 0xbf58476d1ce4e5b9ULL;
        x ^= x >> 27;
        x *= 0x94d049bb133111ebULL;
        x ^= x >> 31;
        return x;
    }

    UUID::UUID()
    {
        // Generate the random ID using a combination of random bits, time, and an atomic counter for uniqueness
        uint64_t rnd = sDistribution(sEngine);
        uint64_t time = GetTimeEntropy();
        uint64_t count = sCounter.fetch_add(1, std::memory_order_relaxed);

        // Single high-entropy 64-bit ID
        uint64_t id = rnd;
        id ^= HashMix(time);
        id ^= HashMix(count);

        mID = HashMix(id);
    }

    UUID::UUID(uint64_t id)
        : mID(id)
    {
    }

    UUID::UUID(const UUID& other)
        : mID(other.mID)
    {
    }

    static const char* sHexChars = "0123456789abcdef";
    String UUID::ToString() const
    {
        String str(16, '0');
        uint64_t value = mID;

        for (int i = 15; i >= 0; --i)
        {
            str[i] = sHexChars[value & 0xF];
            value >>= 4;
        }

        return str;
    }

} // namespace Surge