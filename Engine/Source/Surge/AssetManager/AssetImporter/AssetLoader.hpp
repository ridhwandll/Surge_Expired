// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Memory.hpp"
#include "Surge/AssetManager/AssetBase.hpp"
#include "Surge/AssetManager/AssetImporter/IAssetLoader.hpp"
#include <unordered_map>

namespace Surge
{
    class AssetLoader
    {
    public:
        static void Init();
        static void Shutdown();

        static bool TryLoadData(const AssetMetadata& metadata, Ref<Asset>& asset);
        static bool Serialize(const AssetMetadata& metadata, Ref<Asset>& asset);
    private:
        static std::unordered_map<AssetType, Scope<IAssetLoader>> sLoaders;
    };
}