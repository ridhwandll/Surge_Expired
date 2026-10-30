// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "AssetCooker.hpp"
#include "Surge/Asset/Format/Scene/SceneFormat.hpp"

namespace Surge
{
    class SceneCooker final : public AssetCooker
    {
    public:
        virtual bool Cook(const CookContext& context) const override;
        virtual AssetType GetAssetType() const override { return AssetType::SCENE; }
        virtual Uint GetCookerVersion() const override { return SceneBinary::GetSchemaVersion(); }
    };
}
