// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "AssetCooker.hpp"
#include "Surge/Asset/Serializer/AssetStamp.hpp"

namespace Surge
{
    class FontCooker final : public AssetCooker
    {
    public:
        virtual CookResult Cook(const String& sourceAbsPath, AssetID id) const override;
        virtual AssetType GetAssetType() const override { return AssetType::FONT; }
        virtual Uint GetCookerVersion() const override { return 2; }

        // Engine UI font (EngineFont::SOURCE_PATH -> COOKED_PATH), before Core::Initialize. Its stamp carries a hash of the .ttf instead of
        // the mod time, so a fresh clone (new mod times) keeps the committed file. Bump ENGINE_FONT_VERSION when the atlas settings change
        static void CookEngineFontIfOutdated();

    private:
        static constexpr Uint ENGINE_FONT_VERSION = 1;

        static bool CookAtlas(const String& sourcePath, const String& outputPath, const AssetStamp& stamp, Uint atlasSize, Uint glyphSize, float pxRange);
        static String FindMSDFAtlasGenExe();
    };
}
