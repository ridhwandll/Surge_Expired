// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/AssetManager/AssetImporter/IAssetLoader.hpp"

namespace Surge
{
    class Material;
    class MaterialLoader : public IAssetLoader
    {
    public:
        virtual bool LoadData(AssetMetadata metaData, Ref<Asset>& asset) override;
        virtual bool SaveData(AssetMetadata metaData, const Ref<Asset>& asset) override;
    private:
        void SerializeTexture(const Ref<Material>& mat, const char* mapName/*, YAML::Emitter& out*/) const;
    };
}