// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Asset/AssetMetadata.hpp"
#include "Surge/Core/Path.hpp"
#include "Surge/Core/Vector.hpp"
#include <unordered_map>

namespace Surge
{
    // RegistryBinary (Internal/Registry.rREG): every registered asset of an exported game, written by the Editor's export, read by the
    // AssetManager when it starts (CookedHeader of type NONE). The Editor works from the text registry (AssetRegistry.surge) instead
    // Entries       Uint count + (AssetID, AssetType, RelativePath) per asset
    namespace RegistryBinary
    {
        inline constexpr uint16_t VERSION = 1;
        inline constexpr const char* FILE_NAME = "Internal/Registry.rREG"; // Relative to the assets directory

        bool Write(const Path& path, const std::unordered_map<AssetID, AssetMetadata>& registry);
        bool Read(const Path& path, Vector<AssetMetadata>& outEntries); // Only ID, Type and RelativePath are set
    }

} // namespace Surge
