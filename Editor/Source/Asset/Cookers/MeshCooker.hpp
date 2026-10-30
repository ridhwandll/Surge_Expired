// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "AssetCooker.hpp"

namespace Surge
{
    class MeshCooker final : public AssetCooker
    {
    public:
        virtual bool Cook(const CookContext& context) const override;
        virtual AssetType GetAssetType() const override { return AssetType::MESH; }
        virtual Uint GetCookerVersion() const override { return 6; } // 4: embedded (.glb) textures, 5: skeleton, skin and clips, 6: overrides from the import settings
    };
}