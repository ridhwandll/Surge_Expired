// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Raptor/Raptor.hpp"
#include "Surge/Core/UUID.hpp"
#include <type_traits>

// Every binary format (cooked assets, shaders) reads and writes through Rapt0R (Surge/Core/Raptor): fused Writer::Write / Reader::Read
// calls and sticky errors, so a whole file is written or read and checked once at the end. Reads are bounds checked and fail cleanly on
// truncated or stale files. Strings and vectors are a Uint count + payload, bools one validated byte, trivially copyable types
// (glm, UUID, enums, POD structs) their raw bytes. Little endian like every target (x64, arm64)

// Declares the Rapt0RSerialize/Rapt0RDeserialize pair of Type from one field list written in terms of `v`, so the writer and the reader
// of a format can never disagree on the order. Use it in the namespace of Type (Rapt0R finds the pair through ADL), before any Write/Read of it
#define SURGE_RAPTOR_FIELDS(Type, ...)                                                                          \
    [[maybe_unused]] static void Rapt0RSerialize(Rapt0R::Writer& w, const Type& v) { w.Write(__VA_ARGS__); } \
    [[maybe_unused]] static void Rapt0RDeserialize(Rapt0R::Reader& r, Type& v) { r.Read(__VA_ARGS__); }

// A type written as raw bytes: trivially copyable with this exact size on every platform and build. A new field, new padding or a bool
// (raw reads don't validate bools) breaks the build here instead of the cooked files: give the type a SURGE_RAPTOR_FIELDS list, or bump its format VERSION and update the size
#define SURGE_RAPTOR_RAW(Type, Size) \
    static_assert(std::is_trivially_copyable_v<Type> && sizeof(Type) == (Size), #Type " is written as raw bytes: update its binary format (+ VERSION), then this size")

namespace Surge
{
    SURGE_RAPTOR_RAW(UUID, 8); // AssetIDs and entity IDs in every format

    // Rapt0R reads enums as raw bytes: readers check them against the last enumerator once read (and update that when the enum grows),
    // a stale/corrupt file must not produce out of range values. Negative values wrap around and fail too
    template <typename E>
    constexpr bool IsEnumInRange(E value, E last)
    {
        using U = std::make_unsigned_t<std::underlying_type_t<E>>;
        return static_cast<U>(value) <= static_cast<U>(last);
    }
}
