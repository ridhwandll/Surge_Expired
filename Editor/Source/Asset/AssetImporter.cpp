// Copyright (c) - SurgeTechnologies - All rights reserved
#include "AssetImporter.hpp"
#include "Surge/Utility/Filesystem.hpp"

namespace Surge
{
    void AssetImporter::Initialize(AssetManager* am, Scope<AssetCookerRegistry> cookers)
    {
        mAssetManager = am;
        mCookers = std::move(cookers);
    }

    void AssetImporter::Shutdown() {}

    void AssetImporter::ScanAndCook() const
    {
        Uint cooked = 0, skipped = 0;

        for(const auto& [id, meta] : mAssetManager->GetRegistryMap())
        {
            if(HasFlag(meta.Flags, AssetFlags::MEMORY) || meta.Type == AssetType::SCENE)
                continue;

            const String absPath = mAssetManager->GetAbsolutePath(meta.RelativePath);
            if(!mCookers->NeedsCook(id, meta.Type))
            {
                skipped++;
                continue;
            }

            const CookResult r = mCookers->Cook(id, meta, mAssetManager);
            if (r.Success)
                cooked++;
            else
                Log<Severity::Error>("[AssetImporter] Cook failed: '{}'", meta.RelativePath);
        }

        Log<Severity::Info>("[AssetImporter] Startup cook complete cooked: {} | already up to date: {}", cooked, skipped);
    }

    Pair<AssetID, CookResult> AssetImporter::ImportAndCook(const String& sourceAbsPath, AssetType type) const
    {
        const String relPath = Filesystem::GetRelativePath(sourceAbsPath, mAssetManager->GetAssetsDirectory()).generic_string();
        const AssetID id = mAssetManager->Import(relPath, type);

        if(!id.IsValid())
        {
            Log<Severity::Error>("[AssetImporter] Import failed: '{}'", sourceAbsPath);
            return {};
        }
        const AssetMetadata& meta = mAssetManager->GetMetadata(id);
        const CookResult r = mCookers->Cook(id, meta, mAssetManager);
        if(!r.Success)
            Log<Severity::Error>("[AssetImporter] Cook failed after import: {}", sourceAbsPath);

        return { id, r };
    }

    CookResult AssetImporter::RecookAsset(AssetID id)
    {
        const AssetMetadata& meta = mAssetManager->GetMetadata(id);
        mAssetManager->Unload(id);
        return mCookers->Cook(id, meta, mAssetManager);
    }

}