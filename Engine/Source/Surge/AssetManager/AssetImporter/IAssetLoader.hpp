// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/AssetManager/AssetMetadata.hpp"

namespace Surge
{
    class IAssetLoader
    {
    public:
        virtual ~IAssetLoader() = default;

        virtual bool LoadData(AssetMetadata metaData, Ref<Asset>& asset) = 0;
        virtual bool SaveData(AssetMetadata metaData, const Ref<Asset>& asset) = 0;
    };
}