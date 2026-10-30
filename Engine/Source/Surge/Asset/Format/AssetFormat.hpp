// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Asset/Asset.hpp"
#include "Surge/Asset/AssetMetadata.hpp"
#include "Surge/Asset/Format/CookedFile.hpp"

namespace Rapt0R
{
    class Writer;
}

namespace Surge
{
    struct AssetContext;

    // The cooked binary format of one asset type, owned by the engine in both directions: the AssetManager reads with it, the Editor writes
    // with it (cookers write the same payload from source data). The AssetManager reads the file and checks its CookedHeader first
    class AssetFormat
    {
    public:
        virtual ~AssetFormat() = default;

        virtual AssetType GetType() const = 0;
        virtual uint16_t GetFormatVersion() const = 0;

        // Null when the payload is corrupt (the format logs why), the asset is then marked MISSING
        virtual Ref<Asset> Read(CookedFile& file, const AssetMetadata& metadata, const AssetContext& context) const = 0;

        // The payload of a live asset, for the Editor's Save. False for types only a cooker can produce, from their source file
        virtual bool Write([[maybe_unused]] Rapt0R::Writer& payload, [[maybe_unused]] const Asset& asset) const { return false; }

        // Built-in assets ("Engine://" paths) are made in code instead of read from a file
        virtual Ref<Asset> CreateBuiltin([[maybe_unused]] const AssetMetadata& metadata, [[maybe_unused]] const AssetContext& context) const { return nullptr; }
    };

} // namespace Surge
