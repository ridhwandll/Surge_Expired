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
#include <bit>
#include <cstddef>
#include <cstdint>

#define R0R_ND [[nodiscard]]

#if defined(_MSC_VER)
#define R0R_ALLOC_ATTR __declspec(allocator) __declspec(restrict)
#define R0R_RESTRICT __restrict
#define R0R_INLINE __forceinline
#define R0R_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define R0R_ALLOC_ATTR __attribute__((malloc))
#define R0R_RESTRICT __restrict__
#define R0R_INLINE inline __attribute__((always_inline))
#define R0R_NOINLINE __attribute__((noinline))
#else
#define R0R_ALLOC_ATTR
#define R0R_RESTRICT
#define R0R_INLINE inline
#define R0R_NOINLINE
#endif

namespace Rapt0R
{
    // The wire format is raw native memory, so it is only portable between little-endian targets
    static_assert(std::endian::native == std::endian::little, "Rapt0R currently requires a little-endian target");

    using Byte = uint8_t;

    // Lengths and element counts are encoded as uint32_t on the wire
    inline constexpr size_t MAX_ELEMENT_COUNT = UINT32_MAX;

    enum class ErrorCode : uint8_t
    {
        SUCCESS = 0,
        FAILED,
        BUFFER_OVERFLOW,
        CORRUPT_DATA,
        SIZE_OVERFLOW,   // A length or element count does not fit in the uint32_t wire encoding
        MISALIGNED_DATA  // A zero-copy span would point at memory not aligned for its element type
    };

    struct Result
    {
        ErrorCode Error = ErrorCode::FAILED;
        uint64_t BytesProcessed = 0;
        constexpr explicit operator bool() const noexcept { return Error == ErrorCode::SUCCESS; }
    };
}