// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Asset.hpp"
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/String.hpp"

namespace Surge
{
    enum class AssetFlags : Uint
    {
        NONE = 0,
        VALID = BIT(0),   // Registered and source file present on disk
        LOADED = BIT(1),
        MISSING = BIT(2), // Source file could not be located
        MEMORY = BIT(3),  // Loaded from memory
    };
    inline AssetFlags  operator| (AssetFlags a, AssetFlags b) { return AssetFlags(uint32_t(a) | uint32_t(b)); }
    inline AssetFlags  operator& (AssetFlags a, AssetFlags b) { return AssetFlags(uint32_t(a) & uint32_t(b)); }
    inline AssetFlags& operator|=(AssetFlags& a, AssetFlags b) { return a = a | b; }
    inline AssetFlags& operator&=(AssetFlags& a, AssetFlags b) { return a = a & b; }
    inline AssetFlags  operator~ (AssetFlags a) { return AssetFlags(~uint32_t(a)); }
    inline bool HasFlag(AssetFlags flags, AssetFlags test) { return (flags & test) != AssetFlags::NONE; }

    // AssetMetadata per-asset registry entry, never leaves the AssetManager
    struct AssetMetadata
    {
        AssetID ID = UUID::INVALID;
        AssetType Type = AssetType::NONE;
        AssetFlags Flags = AssetFlags::NONE;
        String RelativePath; // Relative to the assets directory OR memoryStr if loaded from memory

        bool IsValid() const { return HasFlag(Flags, AssetFlags::VALID); }
        bool IsLoaded() const { return HasFlag(Flags, AssetFlags::LOADED); }
        bool IsMissing() const { return HasFlag(Flags, AssetFlags::MISSING); }
    };

} // namespace Surge