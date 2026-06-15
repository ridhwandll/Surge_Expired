// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include <cstdint>
#include <type_traits>
#include <compare>

namespace Surge
{
    class UUID
    {
    public:
        inline static constexpr uint64_t INVALID = 0;

    public:
        UUID();
        UUID(uint64_t id)
            : mID(id) {}

        UUID(const UUID&) = default;
        UUID& operator=(const UUID&) = default;

        uint64_t Get() const { return mID; }
        bool IsValid() const { return mID != INVALID; }
        void MakeInvalid() { mID = INVALID; }

        operator bool() const { return IsValid(); }
        operator uint64_t() { return mID; }
        auto operator<=>(const UUID&) const = default;
    private:
        uint64_t mID;
    };

} // namespace Surge


namespace std
{
    template <>
    struct hash<Surge::UUID>
    {
        size_t operator()(const Surge::UUID& uuid) const noexcept
        {
            return static_cast<size_t>(uuid.Get());
        }
    };

} // namespace std