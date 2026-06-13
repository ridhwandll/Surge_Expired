// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "AssetCooker.hpp"

namespace Surge
{
    class Texture2DCooker : public AssetCooker
    {
    public:
        virtual CookResult Cook(const String& sourceAbsPath, AssetID id) const override;
        virtual AssetType GetAssetType() const override { return AssetType::TEXTURE2D; }
        virtual Uint GetCookerVersion() const override { return 2; }

    private:
        //Ref<Asset> LoadFromSource(const String& absPath) const;
    private:
        static String FindBasisuExe();
        static bool IsLinearColorSpace(const String& sourceAbsPath);
    };
}