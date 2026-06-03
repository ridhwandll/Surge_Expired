// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Asset.hpp"
#include "Surge/Graphics/RHI/RHIHandle.hpp"

namespace Surge
{
    class Texture2D : public Asset
    {
    public:
        Texture2D(const String& path);
        ~Texture2D();

        virtual AssetType GetAssetType() const override { return AssetType::TEXTURE2D; }

        ImageHandle GetRHIImage() { return mImageHandle; }

        static Ref<Texture2D> Create(const String& path);
    private:
        ImageHandle mImageHandle;
    };

}