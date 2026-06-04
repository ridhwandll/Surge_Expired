// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Asset.hpp"
#include "Surge/Graphics/RHI/RHIHandle.hpp"

namespace Surge
{
    struct TextureLoadData
    {
        Byte* Content = nullptr;
        Uint Width = 0;
        Uint Height = 0;
        Uint Channels = 0;
    };

    class Texture2D : public Asset
    {
    public:
        Texture2D(const String& path);
        ~Texture2D();

        virtual AssetType GetAssetType() const override { return AssetType::TEXTURE2D; }

        ImageHandle GetRHIImage() { return mImageHandle; }

        static Ref<Texture2D> Create(const String& path);
        static TextureLoadData LoadData(const String& path);
        static void FreeData(TextureLoadData& data);
    private:
        ImageHandle mImageHandle;
    };

}