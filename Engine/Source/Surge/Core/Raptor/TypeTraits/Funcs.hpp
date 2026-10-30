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
#include "../Defines.hpp"
#include "TypeTraits.hpp"
#include "Vector.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#define RAPTOR_SERIALIZE_FUNC Rapt0RSerialize
#define RAPTOR_DESERIALIZE_FUNC Rapt0RDeserialize

namespace Rapt0R
{
    class Writer;
    class Reader;

    // Reader
    template <typename T, typename ReaderType>
    concept HasMemberDeserialize = requires(T val, ReaderType& ar)
    {
        { val.RAPTOR_DESERIALIZE_FUNC(ar) };
    };

    template <typename T, typename ReaderType>
    concept HasFreeDeserialize = requires(T val, ReaderType& ar)
    {
        { RAPTOR_DESERIALIZE_FUNC(ar, val) };
    };

    // Writer
    template <typename T, typename WriterType>
    concept HasMemberSerialize = requires(const T& val, WriterType& ar)
    {
        { val.RAPTOR_SERIALIZE_FUNC(ar) };
    };

    template <typename T, typename WriterType>
    concept HasFreeSerialize = requires(const T& val, WriterType& ar)
    {
        { RAPTOR_SERIALIZE_FUNC(ar, val) };
    };

    // A type with a serializer in either direction must never be blitted, or Writer and Reader would disagree on its format
    template <typename T>
    concept HasCustomSerialization = HasMemberSerialize<T, Writer> || HasFreeSerialize<T, Writer> ||
                                     HasMemberDeserialize<T, Reader> || HasFreeDeserialize<T, Reader>;

    // POD-blittable AND not overridden by a user serializer. Every raw memcpy fast path (vector payloads, std::array,
    // pair/map entries) must use this instead of IsPODBlittable, otherwise e.g. std::vector<T> would bypass T's serializer.
    template <typename T>
    struct IsRawCopyable : std::bool_constant<IsPODBlittable_V<T> && !HasCustomSerialization<T>> {};

    template <typename T, size_t N>
    struct IsRawCopyable<std::array<T, N>> : IsRawCopyable<std::remove_cv_t<T>> {};

    template <typename T, size_t N>
    struct IsRawCopyable<T[N]> : IsRawCopyable<std::remove_cv_t<T>> {};

    template <typename T>
    inline constexpr bool IsRawCopyable_V = IsRawCopyable<std::remove_cvref_t<T>>::value;

    template <typename T>
    struct IsStdOptional : std::false_type {};

    template <typename T>
    struct IsStdOptional<std::optional<T>> : std::true_type {};

    template <typename T>
    struct IsStdPair : std::false_type {};

    template <typename A, typename B>
    struct IsStdPair<std::pair<A, B>> : std::true_type {};

    template <typename T>
    inline constexpr bool IsStdPair_V = IsStdPair<std::remove_cvref_t<T>>::value;

    // I-th argument of a pack, by reference. Unlike std::forward_as_tuple this needs no storage, so the compiler never has
    // to reload the references after a memcpy into Byte memory (which may alias anything, including a tuple on the stack)
    template <size_t I, typename T, typename... Rest>
    R0R_INLINE constexpr decltype(auto) NthArg(T& first, Rest&... rest) noexcept
    {
        if constexpr(I == 0)
            return (first);
        else
            return NthArg<I - 1>(rest...);
    }

    template <size_t I, typename... Ts>
    using NthType = std::remove_cvref_t<std::tuple_element_t<I, std::tuple<Ts...>>>;

    // "Flat" values have an O(1) wire size and no nested serializer calls. Writer::Write / Reader::Read fuse the bounds
    // checks of consecutive flat values and copy them through a register-resident cursor.
    enum class FlatKind : uint8_t
    {
        NONE,          // Anything else: serialized through Writer::WriteValue / Reader::ReadValue
        RAW,           // Raw-copyable value, sizeof(T) bytes
        BOOL,          // 1 byte, validated on read
        STRING,        // uint32_t length + chars (std::basic_string<char> or std::string_view)
        RAW_VECTOR,    // uint32_t count + raw elements
        RAW_OPTIONAL,  // uint8_t flag + raw value if engaged
        FLAT_CONTAINER // Writer only: uint32_t count + flat elements (std::vector of strings, maps/sets of flat keys/values)
    };

    // Containers larger than this are written element by element instead of being sized in a separate first pass,
    // so big node-based containers are never walked twice
    inline constexpr size_t SMALL_CONTAINER_LIMIT = 64;

    template <typename T>
    consteval FlatKind GetFlatKind()
    {
        using R = std::remove_cvref_t<T>;
        if constexpr(std::is_same_v<R, bool>)
            return FlatKind::BOOL;
        else if constexpr(IsCharArray_V<R>)
            return FlatKind::NONE; // Written as strings by Writer::WriteValue
        else if constexpr(IsRawCopyable_V<R>)
            return FlatKind::RAW;
        else if constexpr(std::is_same_v<R, std::string_view> || IsStdString_V<R>)
            return FlatKind::STRING;
        else if constexpr(IsStandardVector_V<R>)
        {
            using E = typename R::value_type;
            return (IsRawCopyable_V<E> && !std::is_same_v<E, bool>) ? FlatKind::RAW_VECTOR : FlatKind::NONE;
        }
        else if constexpr(IsStdOptional<R>::value)
        {
            using E = typename R::value_type;
            return (IsRawCopyable_V<E> && !std::is_same_v<E, bool>) ? FlatKind::RAW_OPTIONAL : FlatKind::NONE;
        }
        else
            return FlatKind::NONE;
    }

    template <typename T>
    inline constexpr FlatKind FlatKind_V = GetFlatKind<T>();

    // Node-based containers with a Rapt0R serializer (std::map, std::set and their unordered variants)
    template <typename T>
    concept IsNodeContainer = requires { typename T::key_type; typename T::node_type; } && HasFreeSerialize<T, Writer>;

    template <typename T>
    consteval FlatKind GetWriterFlatKind();

    template <typename T>
    inline constexpr FlatKind WriterFlatKind_V = GetWriterFlatKind<T>();

    // Writer-side classification: everything FlatKind_V knows, plus containers whose elements are all flat. Their size
    // is known after one walk, which is what lets the Writer fuse them into the surrounding run. The Reader never treats
    // containers as flat (it has to allocate for them anyway).
    template <typename T>
    consteval FlatKind GetWriterFlatKind()
    {
        using R = std::remove_cvref_t<T>;
        constexpr FlatKind kind = FlatKind_V<R>;
        if constexpr(kind != FlatKind::NONE)
            return kind;
        else if constexpr(IsStandardVector_V<R>)
        {
            using E = typename R::value_type;
            return (!std::is_same_v<E, bool> && WriterFlatKind_V<E> != FlatKind::NONE) ? FlatKind::FLAT_CONTAINER : FlatKind::NONE;
        }
        else if constexpr(IsNodeContainer<R>)
        {
            using E = typename R::value_type;
            if constexpr(IsStdPair_V<E>)
                return (WriterFlatKind_V<typename E::first_type> != FlatKind::NONE && WriterFlatKind_V<typename E::second_type> != FlatKind::NONE)
                    ? FlatKind::FLAT_CONTAINER : FlatKind::NONE;
            else
                return WriterFlatKind_V<E> != FlatKind::NONE ? FlatKind::FLAT_CONTAINER : FlatKind::NONE;
        }
        else
            return FlatKind::NONE;
    }
}
