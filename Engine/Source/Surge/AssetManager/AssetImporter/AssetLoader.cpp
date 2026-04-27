// Copyright (c) - SurgeTechnologies - All rights reserved
#include "AssetLoader.hpp"
#include "TextureLoader.hpp"
#include "MaterialLoader.hpp"

namespace Surge
{
    std::unordered_map<AssetType, Scope<IAssetLoader>> AssetLoader::sLoaders;

    void AssetLoader::Init()
    {
        sLoaders[AssetType::TEXTURE2D] = CreateScope<TextureLoader>();
        sLoaders[AssetType::ENVIRONMENT_MAP] = CreateScope<EnvMapLoader>();
        sLoaders[AssetType::MATERIAL] = CreateScope<MaterialLoader>();
    }

    void AssetLoader::Shutdown()
    {
        sLoaders.clear();
    }

    bool AssetLoader::TryLoadData(const AssetMetadata& metadata, Ref<Asset>& asset)
    {
        if (sLoaders.find(metadata.Type) == sLoaders.end())
        {
            if(metadata.Handle == INVALID_ASSET_HANDLE && metadata.IsDataLoaded == false && metadata.Type == AssetType::NONE && metadata.Path == "")
                return false;

            Log<Severity::Warn>("There is currently no loaders for assets of type {0}", metadata.Path.extension().string());
            return false;
        }

        return sLoaders[metadata.Type]->LoadData(metadata, asset);
    }

    bool AssetLoader::Serialize(const AssetMetadata& metadata, Ref<Asset>& asset)
    {
        if (sLoaders.find(metadata.Type) == sLoaders.end())
        {
            Log<Severity::Warn>("There is currently no serializer for assets of type {0}", metadata.Path.extension().string());
            return false;
        }

        return sLoaders[metadata.Type]->SaveData(metadata, asset);
    }
}