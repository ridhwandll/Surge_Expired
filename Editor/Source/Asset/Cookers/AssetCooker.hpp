// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Asset/AssetMetadata.hpp"
#include "Surge/Asset/Format/CookedFile.hpp"
#include "Surge/Core/String.hpp"
#include "Asset/ImportSettings.hpp"

namespace Rapt0R
{
    class Writer;
}

namespace Surge
{
    struct CookResult
    {
        bool Success = false;
        String OutputPath;
        float InputMegaBytes = 0;
        float OutputMegaBytes = 0;
    };

    // Everything a cooker gets from the AssetDatabase for one asset
    struct CookContext
    {
        AssetID ID;
        AssetMetadata Metadata; // A copy: cooking can register other assets (a model's textures)
        Path SourcePath;        // Absolute
        Path OutputPath;        // The cooked file, Internal/<TYPE>/<id>.r<TYPE>
        ImportSettings Settings;
        CookedHeader Header;    // Type, CookerVersion and the freshness of the inputs, filled in

        // Writes Header + payload to OutputPath, the payload built by the format's own Write (MeshBinary::Write & co)
        bool Write(uint16_t formatVersion, const Rapt0R::Writer& payload) const;
    };

    // Turns one source file type into the engine's cooked format. Owned and driven by the AssetDatabase, which decides when to cook
    class AssetCooker
    {
    public:
        virtual ~AssetCooker() = default;

        virtual bool Cook(const CookContext& context) const = 0;
        virtual AssetType GetAssetType() const = 0;
        virtual Uint GetCookerVersion() const = 0;

        // Inputs besides the source file and its .import, hashed into CookedHeader::InputHash (scripts: their asset path, baked into chunk names)
        virtual String GetExtraInputs([[maybe_unused]] const AssetMetadata& metadata) const { return {}; }
    };
}
