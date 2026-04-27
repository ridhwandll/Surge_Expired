// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/AssetManager/AssetImporter/IAssetLoader.hpp"

namespace Surge
{
    class TextureLoader : public IAssetLoader
    {
    public:
        virtual bool LoadData(AssetMetadata metaData, Ref<Asset>& asset) override;
        virtual bool SaveData(AssetMetadata metaData, const Ref<Asset>& asset) override;
    };

    class EnvMapLoader : public IAssetLoader
    {
    public:
        virtual bool LoadData(AssetMetadata metaData, Ref<Asset>& asset) override;
        virtual bool SaveData(AssetMetadata metaData, const Ref<Asset>& asset) override;
    };
}
