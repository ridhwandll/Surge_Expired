// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "AssetCooker.hpp"

namespace Surge
{
    class FontCooker final : public AssetCooker
    {
    public:
        virtual bool Cook(const CookContext& context) const override;
        virtual AssetType GetAssetType() const override { return AssetType::FONT; }
        virtual Uint GetCookerVersion() const override { return 3; }

        // Engine UI font (EngineFont::SOURCE_PATH -> COOKED_PATH), before Core::Initialize. Its header carries a hash of the .ttf instead of
        // the mod time, so a fresh clone (new mod times) keeps the committed file. Bump ENGINE_FONT_VERSION when the atlas settings change
        static void CookEngineFontIfOutdated();

    private:
        static constexpr Uint ENGINE_FONT_VERSION = 3; // 3: CookedHeader

        static bool CookAtlas(const String& sourcePath, const String& outputPath, const CookedHeader& header, Uint atlasSize, Uint glyphSize, float pxRange);
        static String FindMSDFAtlasGenExe();
    };
}
