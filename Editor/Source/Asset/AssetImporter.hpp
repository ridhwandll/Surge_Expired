// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Asset/Cookers/AssetCookerRegistry.hpp"

namespace Surge
{
    class AssetManager;
    class AssetImporter
    {
    public:
        void Initialize(AssetManager* am, Scope<AssetCookerRegistry> cookers);
        void Shutdown();

        // Called once after AssetManager::Initialize()
        // Scans every registered asset, cooks any that are missing a cooked file
        void ScanAndCook() const;

        // Cooks a single just-imported source file immediately
        Pair<AssetID, CookResult> ImportAndCook(const String& sourceAbsPath, AssetType type) const;

        CookResult RecookAsset(AssetID id);

        AssetCookerRegistry* GetCookerRegistry() { return mCookers.get(); }

    private:
        AssetManager* mAssetManager = nullptr;
        Scope<AssetCookerRegistry> mCookers;
    };
}