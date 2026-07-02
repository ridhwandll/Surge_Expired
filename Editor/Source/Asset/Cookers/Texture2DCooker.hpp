// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "AssetCooker.hpp"
#include "Surge/Graphics/HighLevel/Texture2D.hpp"

namespace Surge
{
    class Texture2DCooker final : public AssetCooker
    {
    public:
        virtual CookResult Cook(const String& sourceAbsPath, AssetID id) const override;
        virtual AssetType GetAssetType() const override { return AssetType::TEXTURE2D; }
        virtual Uint GetCookerVersion() const override { return 2; }

        // LoadFromSource
        // Loads a texture from a source file (PNG, JPG, etc.) into a TextureSpecification struct
        // The caller is responsible for freeing the Content pointer in the TextureSpecification using Texture2DCooker::FreeLoadedSource()
        // No Mips are generated from source
        // @param absPath The absolute path to the source texture file
        // @return        A TextureSpecification struct containing the loaded texture data
        static TextureSpecification LoadFromSource(const String& absPath);

        static void FreeLoadedSource(TextureSpecification sourceData);

    private:
        static String FindBasisuExe();
        static bool IsLinearColorSpace(const String& sourceAbsPath);
    };
}