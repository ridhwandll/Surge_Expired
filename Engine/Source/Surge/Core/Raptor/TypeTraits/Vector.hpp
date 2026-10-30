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
#include <vector>
#include <type_traits>

namespace Rapt0R
{
    template <typename T>
    struct IsStandardVector : std::false_type {};

    template <typename T, typename Alloc>
    struct IsStandardVector<std::vector<T, Alloc>> : std::true_type {};

    template <typename T>
    inline constexpr bool IsStandardVector_V = IsStandardVector<T>::value;
}