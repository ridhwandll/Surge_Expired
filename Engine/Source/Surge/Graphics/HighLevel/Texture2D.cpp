// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Texture2D.hpp"
#include "Surge/Core/Core.hpp"

namespace Surge
{
    Texture2D::Texture2D(const TextureSpecification& spec)
    {
        Renderer* renderer = Core::GetRenderer();

        ImageDesc desc = {};
        desc.Width = spec.Width;
        desc.Height = spec.Height;
        desc.Format = spec.Format;
        desc.Usage = ImageUsage::SAMPLED | ImageUsage::TRANSFER_DST;
        desc.DebugName = spec.DebugName;
        desc.GenerateImGuiID = true;
        if(spec.GenerateMips)
            desc.MipLevel = static_cast<Uint>(std::floor(std::log2(std::max(spec.Width, spec.Height)))) + 1;
        else
            desc.MipLevel = 1;

        desc.InitialData = spec.Content;
        desc.DataSize = spec.Width * spec.Height * 4;
        desc.Sampler = renderer->GetDefaultSampler();
        mImageHandle = renderer->GetRHI()->CreateImage(desc);
    }

    Texture2D::~Texture2D()
    {
        Core::GetRenderer()->GetRHI()->DestroyImage(mImageHandle);
    }

    Ref<Texture2D> Texture2D::Create(const TextureSpecification& spec)
    {
        return Ref<Texture2D>::Create(spec);
    }
}