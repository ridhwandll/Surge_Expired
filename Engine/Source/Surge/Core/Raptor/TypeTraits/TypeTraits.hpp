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

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace Rapt0R
{
    // NOTE: A trivially copyable struct with a pointer-like member (raw pointer, std::string_view, std::span)
    // still passes this check and would be blitted as an address. Give such types a Rapt0RSerialize/Rapt0RDeserialize,
    // which always takes priority over blitting.
    template <typename T>
    struct IsPODBlittable : std::bool_constant<
        std::is_trivially_copyable_v<T> &&
        std::is_standard_layout_v<T> &&
        !std::is_pointer_v<T> &&
        !std::is_same_v<std::remove_cvref_t<T>, std::string_view>> {};

    // std::array<T, N> is ONLY raw-blittable if its element T is POD-blittable!
    template <typename T, size_t N>
    struct IsPODBlittable<std::array<T, N>> : IsPODBlittable<T> {};

    // std::pair<K, V> is ONLY raw-blittable if both K and V are POD-blittable
    // AND there is no hidden internal struct padding, OR we serialize field-by-field:
    template <typename K, typename V>
    struct IsPODBlittable<std::pair<K, V>> : std::false_type {};

    template <typename... Ts>
    struct IsPODBlittable<std::tuple<Ts...>> : std::false_type {};

    template <typename T>
    struct IsPODBlittable<std::optional<T>> : std::false_type {};

    template <typename... Ts>
    struct IsPODBlittable<std::variant<Ts...>> : std::false_type {};

    // A span is a pointer + size, blitting it would write an address
    template <typename T, size_t Extent>
    struct IsPODBlittable<std::span<T, Extent>> : std::false_type {};

    template <typename T>
    inline constexpr bool IsPODBlittable_V = IsPODBlittable<std::remove_cvref_t<T>>::value;

    // char[N] (string literals, fixed-size C string buffers) is serialized as a length-prefixed string
    template <typename T>
    inline constexpr bool IsCharArray_V = std::is_array_v<T> && std::rank_v<T> == 1 && std::is_same_v<std::remove_cv_t<std::remove_extent_t<T>>, char>;

    template <typename T>
    inline constexpr bool IsCharPointer_V = std::is_same_v<T, const char*> || std::is_same_v<T, char*>;

    // Any std::basic_string<char> regardless of allocator (std::string, std::pmr::string, arena-backed strings)
    template <typename T>
    struct IsStdString : std::false_type {};

    template <typename Alloc>
    struct IsStdString<std::basic_string<char, std::char_traits<char>, Alloc>> : std::true_type {};

    template <typename T>
    inline constexpr bool IsStdString_V = IsStdString<T>::value;
}
