// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Asset/Asset.hpp"
#include "Surge/Core/Path.hpp"
#include <map>

namespace Surge
{
    // How a cooker treats one source file: <source>.import (JSON) next to it, created only when a setting differs from its default.
    // It's source data (committed, moved/renamed/deleted with its source by AssetFileOps) and an input of the cooked file's freshness
    struct ImportSettings
    {
        static constexpr const char* EXTENSION = ".import";

        enum class ColorSpace : uint8_t
        {
            AUTO,  // Linear when the filename looks like a data map (normal, roughness, metallic, AO...), sRGB otherwise
            SRGB,
            LINEAR
        };

        // MESH: material slot -> MATERIAL asset replacing the slot's glTF material
        std::map<Uint, AssetID> MaterialOverrides;

        // TEXTURE2D
        ColorSpace TextureColorSpace = ColorSpace::AUTO;

        bool IsDefault() const { return MaterialOverrides.empty() && TextureColorSpace == ColorSpace::AUTO; }

        // "Models/Knight.glb" -> "Models/Knight.glb.import"
        static Path GetPath(const Path& sourcePath);

        // Defaults when the source has no .import (or it can't be parsed, which is logged)
        static ImportSettings Load(const Path& sourcePath);
        // Default settings delete the file instead
        static bool Save(const Path& sourcePath, const ImportSettings& settings);
    };

} // namespace Surge
