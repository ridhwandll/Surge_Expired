// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Asset/AssetMetadata.hpp"

#include "Asset/Cookers/AssetCooker.hpp"
#include <unordered_map>

namespace Surge
{
    class AssetManager;
    class AssetImporter
    {
    public:
        void Initialize(AssetManager* am);
        void RegisterCooker(Scope<AssetCooker> cooker);
        void Shutdown();

        void ScanAndRecookScripts();
        void ScanAndCookAll() const;
        CookResult RecookAsset(AssetID id);

        bool NeedsCook(AssetID id, AssetType type) const;
        CookResult Cook(AssetID id, const AssetMetadata& meta, AssetManager* am) const;
    private:
        AssetManager* mAssetManager = nullptr;
        std::unordered_map<AssetType, Scope<AssetCooker>> mCookers;
    };
}