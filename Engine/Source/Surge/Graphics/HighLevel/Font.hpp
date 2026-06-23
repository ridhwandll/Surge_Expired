// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Asset/Asset.hpp"
#include "Surge/Graphics/RHI/RHIHandle.hpp"
#include "Surge/Graphics/RHI/RHIDescs.hpp"

namespace Surge
{
    struct FontGlyph
    {
        Uint UnicodeID;
        float Advance;
        glm::vec2 UVBounds[2];
        glm::vec2 PlaneBounds[2];
    };

    struct FontSpecification
    {
        float LineHeight = 0.0f;
        std::unordered_map<Uint, FontGlyph> Glyphs;
        Vector<Byte> KTX2Data;
    };

    class Font final : public Asset
    {
    public:
        Font(FontSpecification&& spec);
        ~Font();
        SURGE_ASSET_TYPE(AssetType::FONT);

        ImageHandle GetAtlas() const { return mAtlasImageHandle; }
        float GetLineHeight() const { return mLineHeight; }
        const FontGlyph* GetGlyph(Uint unicodeID) const;
        const FontSpecification& GetSpecification() const { return mSpecification; }

        static Ref<Font> Create(FontSpecification&& spec);
    private:
        ImageHandle mAtlasImageHandle;
        float mLineHeight = 0.0f;
        std::unordered_map<Uint, FontGlyph> mGlyphs;
        FontSpecification mSpecification;
    };

}