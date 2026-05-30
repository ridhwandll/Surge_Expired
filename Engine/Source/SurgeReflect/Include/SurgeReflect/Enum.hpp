// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include <string_view>
#include <array>
#include <utility>
#include <cstdio>
#include <cassert>

namespace SurgeReflect
{
    // (Rid) By default, enums are assumed to be between 0 and 15
    // Specialize this template anywhere in codebase to expand or shift the bounds
    /*
    * Example:
    *
    * enum class Cake
    * {
    *     CHOCOLATE = 400, //EnumTraits<Cake>::Min
    *     VANILLA,
    *     BUTTERSCOTCH,
    *     BLUEBERRY,
    *     CHEESE //404 -> //EnumTraits<Cake>::Max
    * };
    *
    * namespace SurgeReflect
    * {
    *     template <>
    *     struct EnumTraits<Cake>
    *     {
    *         // Define the Min and Max of your enum value here
    *         static constexpr int Min = 400;
    *         static constexpr int Max = 404;
    *     };
    * }
    */
    template <typename T>
    struct EnumTraits
    {
        static constexpr int Min = 0;
        static constexpr int Max = 15;
    };

    namespace Detail
    {
        template <typename T, T Value>
        constexpr std::string_view GetRawName()
        {
#if defined(__clang__) || defined(__GNUC__)
            return __PRETTY_FUNCTION__;
#elif defined(_MSC_VER)
            return __FUNCSIG__;
#else
#error "SurgeReflect: Unsupported Compiler!"
#endif
        }

        template <typename T, T Value>
        constexpr std::string_view EnumToStringV_Raw()
        {
            std::string_view name = GetRawName<T, Value>();
#if defined(__clang__) || defined(__GNUC__)
            // Isolate the "Value = " identifier token
            //(Rid) https://godbolt.org/z/a5aP6Ts3v
            constexpr std::string_view target = "Value = ";
            size_t pos = name.find(target);
            if(pos == std::string_view::npos)
                return {};

            name.remove_prefix(pos + target.length());

            // Chop off everything after the value token (stops at ; or ])
            size_t end_pos = name.find_first_of(";]");
            if(end_pos != std::string_view::npos)
                name.remove_suffix(name.size() - end_pos);

#elif defined(_MSC_VER)
            // MSVC gens: auto __cdecl _67::g<enum Color,Color::RED>(void)
            size_t grand = name.rfind('>');
            if(grand == std::string_view::npos)
                return {};

            name.remove_suffix(name.size() - grand);

            size_t lastComma = name.rfind(',');
            if(lastComma == std::string_view::npos)
                return {};

            name.remove_prefix(lastComma + 1);

            while(!name.empty() && name.front() == ' ')
                name.remove_prefix(1);
#endif

            // Surge::ImageUsage::SAMPLED turns into SAMPLED
            size_t last_colon = name.rfind("::");
            if(last_colon != std::string_view::npos)
                name.remove_prefix(last_colon + 2);

            // Filter out compiler-generated fallback numbers for unmapped values
            if(!name.empty() && ((name[0] >= '0' && name[0] <= '9') || name[0] == '-' || name[0] == '('))
                return {};

            return name;
        }

        template <std::size_t N>
        struct FixedString
        {
            char Data[N + 1] {};
            constexpr FixedString(std::string_view sv)
            {
                for(std::size_t i = 0; i < N; ++i)
                    Data[i] = sv[i];

                Data[N] = '\0';
            }
        };

        template <typename T, T Value, std::size_t N>
        struct NameCache
        {
            // (Rid) Exactly one instance of the string per enum value
            static constexpr FixedString<N> Buffer_ { EnumToStringV_Raw<T, Value>() };
        };

        template <typename T, T Value>
        constexpr std::string_view EnumToStringV()
        {
            constexpr auto raw = EnumToStringV_Raw<T, Value>();
            if constexpr(raw.empty())
                return {};
            else
                return { NameCache<T, Value, raw.size()>::Buffer_.Data, raw.size() };
        }

        template <typename T, int... Is>
        constexpr auto MakeNameTable(std::integer_sequence<int, Is...>)
        {
            return std::array<std::string_view, sizeof...(Is)> {
                EnumToStringV<T, static_cast<T>(Is + EnumTraits<T>::Min)>()...
            };
        }

        template <typename T>
        struct EnumTableCache
        {
            static constexpr int Min = EnumTraits<T>::Min;
            static constexpr int Max = EnumTraits<T>::Max;

            static_assert(Max >= Min, "Error: Max bound must be greater than or equal to Min bound!");
            static_assert(Max - Min <= 50, "Error: Range exceeds 50 values! Split your enum or adjust traits to prevent compiler freezing!");

            static constexpr auto Table = MakeNameTable<T>(std::make_integer_sequence<int, Max - Min + 1>{});
        };
    }

    template <typename T>
    constexpr std::string_view EnumToString(T value)
    {
        constexpr auto& table = Detail::EnumTableCache<T>::Table;
        const auto index = static_cast<int>(value) - EnumTraits<T>::Min;

        if(index < 0 || index >= static_cast<int>(table.size())) [[unlikely]]
        {
            assert(false && "SurgeReflect::EnumToString Enum value out of reflection bounds! Did you forgot to specialize the EnumTraits template?(look at the top of this file)");
            return "SurgeReflect::EnumToString: Unknown Enum value";
        }

        const auto name = table[index];
        return name.empty() ? "SurgeReflect::EnumToString: Unknown Enum value" : name;
    }

} // namespace SurgeReflect