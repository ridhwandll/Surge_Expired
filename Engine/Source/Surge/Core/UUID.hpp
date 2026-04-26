// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/String.hpp"
#define NULL_UUID 0

namespace Surge
{
    class SURGE_API UUID
    {
    public:
        UUID();
        UUID(uint64_t id);
        UUID(const UUID& other);
        uint64_t Get() const { return mID; }
        String ToString() const;

        operator uint64_t() { return mID; }
        operator const uint64_t() const { return mID; }
        bool operator==(const UUID& other) const
        {
            return mID == other.mID;
        }
        bool operator!=(const UUID& other) const
        {
            return mID != other.mID;
        }
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