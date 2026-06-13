// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include <cstdint>
#include <type_traits>

namespace Surge
{
    class UUID
    {
    public:
        inline static constexpr int INVALID = 0;

    public:
        UUID();
        UUID(uint64_t id);
        UUID(const UUID& other);

        uint64_t Get() const { return mID; }
        bool IsValid() const { return mID != INVALID; }
        void MakeInvalid() { mID = INVALID; }

        operator uint64_t() { return mID; }
        operator const uint64_t() const { return mID; }
        bool operator==(const UUID& other) const { return mID == other.mID; }
        bool operator==(const int& other) const { return mID == static_cast<uint64_t>(other); }
        bool operator!=(const UUID& other) const { return mID != other.mID; }
        bool operator!=(const int& other) const { return mID != static_cast<uint64_t>(other); }
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