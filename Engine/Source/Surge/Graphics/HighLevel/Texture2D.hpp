// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Asset/Asset.hpp"
#include "Surge/Graphics/RHI/RHIHandle.hpp"
#include "Surge/Graphics/RHI/RHIDescs.hpp"

namespace Surge
{
    struct TextureSpecification
    {
        Byte* Content = nullptr;
        Uint Width = 0;
        Uint Height = 0;
        ImageFormat Format = ImageFormat::RGBA8_SRGB;
        bool GenerateMips = false;
        String DebugName;
    };

    class Texture2D : public Asset
    {
    public:
        Texture2D(const TextureSpecification& spec);
        ~Texture2D();
        SURGE_ASSET_TYPE(AssetType::TEXTURE2D);

        ImageHandle GetRHIImage() const { return mImageHandle; }

        static Ref<Texture2D> Create(const TextureSpecification& spec);
    private:
        ImageHandle mImageHandle;
    };

}