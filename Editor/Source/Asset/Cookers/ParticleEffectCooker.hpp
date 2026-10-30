// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "AssetCooker.hpp"

namespace Surge
{
    class ParticleEffectCooker final : public AssetCooker
    {
    public:
        virtual bool Cook(const CookContext& context) const override;
        virtual AssetType GetAssetType() const override { return AssetType::PARTICLE_EFFECT; }
        virtual Uint GetCookerVersion() const override { return 2; }
    };
}
