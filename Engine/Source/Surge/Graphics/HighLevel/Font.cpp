// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Font.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"

namespace Surge
{
    Font::Font(Renderer& renderer, FontSpecification&& spec)
        : mRHI(renderer.GetRHI().get()), mSpecification(std::move(spec))
    {
        SG_ASSERT(mSpecification.PxRange >= 2.0f, "Font pixel range must be greater than or equal to 2!");
        SG_ASSERT(mSpecification.LineHeight > 0.0f, "Font line height must be greater than 0!");
        SG_ASSERT(!mSpecification.UncompressedData.empty(), "Font UncompressedData must not be empty!");

        Uint width = mSpecification.Width;
        Uint height = mSpecification.Height;
        SG_ASSERT(width == height, "Font atlas must be square!");

        ImageDesc desc = {};
        desc.Width = width;
        desc.Height = height;

        desc.DebugName = "MSDF Font Atlas";
        desc.GenerateImGuiID = true;
        desc.Sampler = renderer.GetTextSampler();
        desc.Usage = ImageUsage::SAMPLED | ImageUsage::TRANSFER_DST;
        desc.MipLevel = 1;
        desc.Format = ImageFormat::RGBA8_UNORM;

        desc.InitialData = mSpecification.UncompressedData.data();
        desc.DataSize = static_cast<Uint>(mSpecification.UncompressedData.size());

        mAtlasImageHandle = mRHI->CreateImage(desc);
    }

    Font::~Font()
    {
        mRHI->DestroyImage(mAtlasImageHandle);
    }

    const FontGlyph* Font::GetGlyph(Uint unicodeID) const
    {
        auto it = mSpecification.Glyphs.find(unicodeID);
        if(it != mSpecification.Glyphs.end())
            return &it->second;

        return nullptr;
    }

    Ref<Font> Font::Create(Renderer& renderer, FontSpecification&& spec)
    {
        return Ref<Font>::Create(renderer, std::move(spec));
    }
}