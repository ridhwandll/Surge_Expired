// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Asset/Asset.hpp"
#include "Surge/Graphics/RHI/RHIHandle.hpp"
#include "Surge/Graphics/RHI/RHIDescs.hpp"

namespace Surge
{
    struct MipLevelData
    {
        Vector<Byte> Data;
        Uint Width = 0;
        Uint Height = 0;
    };

    struct TextureSpecification
    {
        // TODO: Remove; Texture2D MUST be comporessed and use Mips always even if for a single level (Mips[0] is always guranteed)
        Uint Width = 0;
        Uint Height = 0;

        ImageFormat Format = ImageFormat::RGBA8_SRGB;
        void* Content = nullptr;
        bool GenerateMips = false;
        String DebugName;

        // Only Compressed load paths fill Vector<MipLevelData> Mips;
        // Windows/Android: all levels pre-baked from KTX2 (no GPU mip gen)
        Vector<MipLevelData> Mips;
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