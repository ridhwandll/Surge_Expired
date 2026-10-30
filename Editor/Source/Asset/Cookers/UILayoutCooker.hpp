// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "AssetCooker.hpp"

namespace Surge
{
    class UILayoutCooker final : public AssetCooker
    {
    public:
        virtual bool Cook(const CookContext& context) const override;
        virtual AssetType GetAssetType() const override { return AssetType::UI_LAYOUT; }
        virtual Uint GetCookerVersion() const override { return 2; }
    };
}
