/*
  +------------------------------------------------+
  |  _____             _    ___  _____    01010010 |
  | │  __ ╲           │ │  ╱ _ ╲│  __ ╲   01100001 |
  | │ │__) │__ _ _ __ │ │_│ │ │ │ │__) │  01110000 |
  | │  _  ╱╱ _` │ '_ ╲│ __│ │ │ │  _  ╱   01110100 |
  | │ │ ╲ ╲ (_│ │ │_) │ │_│ │_│ │ │ ╲ ╲   00110000 |
  | │_│  ╲_╲__,_│ .__╱ ╲__│╲___╱│_│  ╲_╲  01010010 |
  |             │ │ A fast and simple C++ 00010100 |
  |             │_│ binary serialization library   |
  +------------------------------------------------+
*/
#pragma once
#include "Defines.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace Rapt0R
{
    inline constexpr size_t CACHE_LINE_ALIGNMENT = 64;

    R0R_ND R0R_ALLOC_ATTR R0R_INLINE void* Malloc(size_t size) noexcept
    {
        return std::malloc(size);
    }

    R0R_ND R0R_INLINE void* Realloc(void* ptr, size_t size) noexcept
    {
        return std::realloc(ptr, size);
    }

    R0R_INLINE void Free(void* ptr) noexcept
    {
        std::free(ptr);
    }

    R0R_ND R0R_ALLOC_ATTR R0R_INLINE void* AlignedMalloc(size_t size, size_t alignment = CACHE_LINE_ALIGNMENT) noexcept
    {
#if defined(_MSC_VER)
        return _aligned_malloc(size, alignment);
#else
        const size_t alignedSize = (size + (alignment - 1)) & ~(alignment - 1);
        return std::aligned_alloc(alignment, alignedSize);
#endif
    }

    R0R_INLINE void AlignedFree(void* ptr) noexcept
    {
#if defined(_MSC_VER)
        _aligned_free(ptr);
#else
        std::free(ptr);
#endif
    }

    R0R_INLINE void Memcopy(void* R0R_RESTRICT dest, const void* R0R_RESTRICT src, size_t count) noexcept
    {
        std::memcpy(dest, src, count);
    }

    template <size_t N>
    R0R_INLINE void Memcopy(void* R0R_RESTRICT dest, const void* R0R_RESTRICT src) noexcept
    {
        if constexpr(N == 0)
            return;
        else if constexpr(N == 1)
            *static_cast<uint8_t*>(dest) = *static_cast<const uint8_t*>(src);
        else
            std::memcpy(dest, src, N);
    }

    // Variable-size copy tuned for the short payloads that dominate real data (names, keys, small arrays). Up to 32 bytes
    // are copied inline with two possibly-overlapping fixed-size moves, avoiding the call into the CRT memcpy whose fixed
    // dispatch cost exceeds the copy itself at these sizes. Larger copies go to memcpy.
    R0R_INLINE void MemcopyVariable(void* R0R_RESTRICT dest, const void* R0R_RESTRICT src, size_t count) noexcept
    {
        Byte* d = static_cast<Byte*>(dest);
        const Byte* s = static_cast<const Byte*>(src);

        if(count <= 16)
        {
            if(count >= 8)
            {
                uint64_t head, tail;
                std::memcpy(&head, s, 8);
                std::memcpy(&tail, s + count - 8, 8);
                std::memcpy(d, &head, 8);
                std::memcpy(d + count - 8, &tail, 8);
            }
            else if(count >= 4)
            {
                uint32_t head, tail;
                std::memcpy(&head, s, 4);
                std::memcpy(&tail, s + count - 4, 4);
                std::memcpy(d, &head, 4);
                std::memcpy(d + count - 4, &tail, 4);
            }
            else if(count > 0)
            {
                // Covers 1, 2 and 3 bytes
                d[0] = s[0];
                d[count >> 1] = s[count >> 1];
                d[count - 1] = s[count - 1];
            }
        }
        else if(count <= 32)
        {
            Byte head[16], tail[16];
            std::memcpy(head, s, 16);
            std::memcpy(tail, s + count - 16, 16);
            std::memcpy(d, head, 16);
            std::memcpy(d + count - 16, tail, 16);
        }
        else
        {
            std::memcpy(d, s, count);
        }
    }

    R0R_INLINE void Memzero(void* R0R_RESTRICT dest, size_t count) noexcept
    {
        std::memset(dest, 0, count);
    }

    // Allocator adaptor that default-initializes instead of value-initializing. std::vector<T, DefaultInitAllocator<T>>::resize
    // leaves trivial elements uninitialized, so Reader::ReadVector writes each byte once instead of zero-filling it first.
    template <typename T, typename Base = std::allocator<T>>
    class DefaultInitAllocator : public Base
    {
        using BaseTraits = std::allocator_traits<Base>;

    public:
        template <typename U>
        struct rebind
        {
            using other = DefaultInitAllocator<U, typename BaseTraits::template rebind_alloc<U>>;
        };

        using Base::Base;
        DefaultInitAllocator() = default;

        template <typename U, typename UBase>
        DefaultInitAllocator(const DefaultInitAllocator<U, UBase>& other) noexcept
            : Base(static_cast<const UBase&>(other)) {}

        template <typename U>
        void construct(U* ptr) noexcept(std::is_nothrow_default_constructible_v<U>)
        {
            ::new(static_cast<void*>(ptr)) U;
        }

        template <typename U, typename... Args>
        void construct(U* ptr, Args&&... args)
        {
            BaseTraits::construct(static_cast<Base&>(*this), ptr, std::forward<Args>(args)...);
        }
    };
}