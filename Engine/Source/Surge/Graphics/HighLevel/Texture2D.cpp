// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Texture2D.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"

namespace Surge
{
    Texture2D::Texture2D(Renderer& renderer, const TextureSpecification& spec)
        : mRHI(renderer.GetRHI().get())
    {
        const bool isCompressed = spec.Format == ImageFormat::ASTC4x4_SRGB || spec.Format == ImageFormat::ASTC4x4_UNORM ||
                                  spec.Format == ImageFormat::BC7_SRGB || spec.Format == ImageFormat::BC7_UNORM ||
                                  spec.Format == ImageFormat::ETC2_RGBA8_SRGB || spec.Format == ImageFormat::ETC2_RGBA8_UNORM;
        const bool hasMips = !spec.Mips.empty();

        SG_ASSERT(isCompressed || spec.Content, "Texture2D: Uncompressed textures must have Content data! Use Mips for compressed textures!");

        ImageDesc desc = {};
        desc.Width = hasMips ? spec.Mips[0].Width : spec.Width;
        desc.Height = hasMips ? spec.Mips[0].Height : spec.Height;
        desc.Format = spec.Format;
        desc.DebugName = spec.DebugName;
        desc.GenerateImGuiID = true;
        desc.Sampler = renderer.GetDefaultSampler();
        desc.Usage = ImageUsage::SAMPLED | ImageUsage::TRANSFER_DST;

        if(spec.GenerateMips && !isCompressed)
            desc.MipLevel = static_cast<Uint>(std::floor(std::log2(std::max(desc.Width, desc.Height)))) + 1;
        else
            desc.MipLevel = hasMips ? static_cast<Uint>(spec.Mips.size()) : 1;

        Vector<MipUploadData> mipUploads;
        if(hasMips)
        {
            mipUploads.resize(spec.Mips.size());
            for(size_t i = 0; i < spec.Mips.size(); i++)
            {
                mipUploads[i].Data = spec.Mips[i].Data.data();
                mipUploads[i].Size = static_cast<Uint>(spec.Mips[i].Data.size());
                mipUploads[i].Width = spec.Mips[i].Width;
                mipUploads[i].Height = spec.Mips[i].Height;
            }

            if(spec.Mips.size() > 1 || isCompressed)
            {
                desc.MipUploads = mipUploads.data();
                desc.MipUploadCount = static_cast<Uint>(mipUploads.size());
            }
            else
            {
                desc.InitialData = mipUploads[0].Data;
                desc.DataSize = mipUploads[0].Size;
            }
        }
        else if(spec.Content)
        {
            desc.InitialData = spec.Content;
            desc.DataSize = desc.Width * desc.Height * 4;
        }

        mImageHandle = mRHI->CreateImage(desc);
    }

    Texture2D::~Texture2D()
    {
        mRHI->DestroyImage(mImageHandle);
    }

    Ref<Texture2D> Texture2D::Create(Renderer& renderer, const TextureSpecification& spec)
    {
        return Ref<Texture2D>::Create(renderer, spec);
    }
}