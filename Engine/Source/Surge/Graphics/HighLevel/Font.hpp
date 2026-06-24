// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Asset/Asset.hpp"
#include "Surge/Graphics/RHI/RHIHandle.hpp"
#include "Surge/Graphics/RHI/RHIDescs.hpp"
#include <glm/vec2.hpp>

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
        std::unordered_map<uint64_t, float> Kerning;
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
        float GetKerning(Uint char1, Uint char2) const
        {
            uint64_t key = ((uint64_t)char1 << 32) | char2;
            auto it = mSpecification.Kerning.find(key);
            return it != mSpecification.Kerning.end() ? it->second : 0.0f;
        }

        static Ref<Font> Create(FontSpecification&& spec);
    private:
        ImageHandle mAtlasImageHandle;
        float mLineHeight = 0.0f;
        std::unordered_map<Uint, FontGlyph> mGlyphs;
        FontSpecification mSpecification;
    };
}
