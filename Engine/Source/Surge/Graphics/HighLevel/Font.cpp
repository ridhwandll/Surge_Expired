// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Font.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Asset/Serializer/Texture2DSerializer.hpp"

namespace Surge
{
    Font::Font(FontSpecification&& spec)
        : mLineHeight(spec.LineHeight), mGlyphs(spec.Glyphs), mSpecification(std::move(spec))
    {
        SG_ASSERT(mSpecification.LineHeight > 0.0f, "Font line height must be greater than 0!");
        SG_ASSERT(!mSpecification.KTX2Data.empty(), "Font KTX2 data must not be empty!");

        TextureSpecification texSpec = Texture2DSerializer::LoadFromKTX2(mSpecification.KTX2Data, "MSDF Font Atlas");
        Uint width = texSpec.Mips[0].Width;
        Uint height = texSpec.Mips[0].Height;

        SG_ASSERT(width == height, "Font atlas must be square!");
        SG_ASSERT(texSpec.Mips.size() == 1, "Font atlas must have only one mip level! (No Mips)");

        Renderer* renderer = Core::GetRenderer();
        ImageDesc desc = {};
        desc.Width = width;
        desc.Height = height;

        desc.DebugName = "MSDF Font Atlas";
        desc.GenerateImGuiID = true;
        desc.Sampler = renderer->GetTextSampler();
        desc.Usage = ImageUsage::SAMPLED | ImageUsage::TRANSFER_DST;
        desc.MipLevel = 1;
        desc.Format = texSpec.Format;

        MipUploadData mipUpload;
        mipUpload.Data = texSpec.Mips[0].Data.data();
        mipUpload.Size = static_cast<Uint>(texSpec.Mips[0].Data.size());
        mipUpload.Width = width;
        mipUpload.Height = height;

        desc.MipUploads = &mipUpload;
        desc.MipUploadCount = 1;

        mAtlasImageHandle = renderer->GetRHI()->CreateImage(desc);

        // mSpecification.KTX2Data.clear(); // TODO: Free the KTX2 data after creating the GPU image
    }

    Font::~Font()
    {
        if(!mAtlasImageHandle.IsNull())
            Core::GetRenderer()->GetRHI()->DestroyImage(mAtlasImageHandle);

        mGlyphs.clear();
    }

    const FontGlyph* Font::GetGlyph(Uint unicodeID) const
    {
        auto it = mGlyphs.find(unicodeID);
        if(it != mGlyphs.end())
            return &it->second;

        return nullptr;
    }

    Ref<Font> Font::Create(FontSpecification&& spec)
    {
        return Ref<Font>::Create(std::move(spec));
    }
}