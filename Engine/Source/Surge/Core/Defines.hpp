// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Logger/Logger.hpp"
#include <deque>
#include <memory>
#include <unordered_map>
#include <vector>

// Platform detection
#ifdef _WIN32
#define SURGE_WINDOWS
#define SCRIPT_API __declspec(dllexport)

#ifdef SURGE_EXPORT
#define SURGE_API
#else
#define SURGE_API
#endif //SURGE_EXPORT

#elif __APPLE__
#define SURGE_APPLE
#error "Haha Apple?"
#define SCRIPT_API "Compiler dependent kekw! Fill this with correct alterantive of __declspec(dllexport)"

#ifdef SURGE_EXPORT
#define SURGE_API kekw
#else
#define SURGE_API kewk
#endif //SURGE_EXPORT

#elif __linux__
#define SURGE_LINUX
#error "Haha LinuS?"
#define SCRIPT_API "Compiler dependent kekw! Fill this with correct alterantive of __declspec(dllexport)"

#ifdef SURGE_EXPORT
#define SURGE_API kekw
#else
#define SURGE_API kewk
#endif //SURGE_EXPORT

#endif

// Assertions
#ifdef SURGE_DEBUG
#define ASSERT() __debugbreak()
#define SG_ASSERT(condition, ...)                              \
    {                                                          \
        if (!(condition))                                      \
        {                                                      \
            ::Surge::Log<Surge::Severity::Fatal>(__VA_ARGS__); \
            ASSERT();                                          \
        }                                                      \
    }
#define SG_ASSERT_NOMSG(condition)                                     \
    {                                                                  \
        if (!(condition))                                              \
        {                                                              \
            ::Surge::Log<Surge::Severity::Fatal>("Assertion Failed!"); \
            ASSERT();                                                  \
        }                                                              \
    }
#define SG_ASSERT_INTERNAL(...)                            \
    {                                                      \
        ::Surge::Log<Surge::Severity::Fatal>(__VA_ARGS__); \
        ASSERT();                                          \
    }
#define SCOPED_TIMER(...) Timer tImEr(fmt::format(__VA_ARGS__), true)
#else
#define ASSERT()
#define SG_ASSERT(...)
#define SG_ASSERT_NOMSG(...)
#define SG_ASSERT_INTERNAL(...)
#define SCOPED_TIMER(...) Timer tImEr(fmt::format(__VA_ARGS__), true)
#endif

// Defines and stuff, TODO: Support for more compilers
#define BIT(x) (1 << x)
#define FORCEINLINE __forceinline
#define NODISCARD [[nodiscard]]
#define MAKE_BIT_ENUM(type)                                                                                                    \
    FORCEINLINE type operator|(type a, type b) { return static_cast<type>(static_cast<int>(a) | static_cast<int>(b)); }        \
    FORCEINLINE type& operator|=(type& a, type b) { return a = static_cast<type>(static_cast<int>(a) | static_cast<int>(b)); } \
    FORCEINLINE bool operator&(type a, type b) { return static_cast<bool>(static_cast<int>(a) & static_cast<int>(b)); }

#define SURGE_DISABLE_COPY(CLASS) \
public:                           \
    CLASS(const CLASS&) = delete; \
    CLASS& operator=(const CLASS&) = delete

#define SURGE_DISABLE_COPY_AND_MOVE(CLASS) \
public:                                    \
    CLASS(const CLASS&) = delete;          \
    CLASS(CLASS&&) = delete;               \
    CLASS& operator=(CLASS&&) = delete;    \
    CLASS& operator=(const CLASS&) = delete

//TODO: Maybe move to a platform specific file
// Platform specific macros
#define ENABLE_WIN32_DEBUG_MESSAGE 0
#if defined(SURGE_WINDOWS) && defined(SURGE_DEBUG) && (ENABLE_WIN32_DEBUG_MESSAGE == 1)
#define SURGE_GET_WIN32_LAST_ERROR                                                                                                                                                  \
    {                                                                                                                                                                               \
        DWORD err = GetLastError();                                                                                                                                                 \
        LPSTR buffer;                                                                                                                                                               \
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, err, 0, reinterpret_cast<LPSTR>(&buffer), 0, nullptr); \
        ::Surge::Log<Surge::Severity::Debug>("[Windows] Function: {0} - File: {1}, at line {2} - Message: {3}", __FUNCTION__, __FILE__, __LINE__, buffer);                          \
    }
#else
#define SURGE_GET_WIN32_LAST_ERROR
#endif // SURGE_WINDOWS

namespace Surge
{
    using Uint = uint32_t;
    using Byte = uint8_t;

    template <typename T>
    using Vector = std::vector<T>; // TODO(Rid): Have a dedicated vector class SURGE_API

    template <typename T>
    using Deque = std::deque<T>;

    template <typename T>
    using Scope = std::unique_ptr<T>;
    template <typename T, typename... Args>
    constexpr Scope<T> CreateScope(Args&&... args)
    {
        return std::make_unique<T>(std::forward<Args>(args)...);
    }

    template <typename T1, typename T2>
    using HashMap = std::unordered_map<T1, T2>;

    template <typename T1, typename T2>
    struct SURGE_API Pair
    {
        T1 Data1;
        T2 Data2;
    };

} // namespace Surge