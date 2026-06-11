// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "AssetCooker/IAssetCooker.hpp"

namespace Surge
{
    class Texture2DCooker : public IAssetCooker
    {
    public:
        virtual CookResult Cook(const String& sourceAbsPath, AssetID id) const override;
        virtual bool NeedsCook(AssetID id) const override;
        virtual AssetType GetAssetType() const override { return AssetType::TEXTURE2D; }

    private:
        static String FindBasisuExe();
        static bool IsLinearColorSpace(const String& sourceAbsPath);
    };
}