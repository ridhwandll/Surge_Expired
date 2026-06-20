// Copyright (c) - SurgeTechnologies - All rights reserved
#include "AssetImporter.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Utility/Filesystem.hpp"

namespace Surge
{
    void AssetImporter::Initialize(AssetManager* am)
    {
        mAssetManager = am;
    }

    void AssetImporter::RegisterCooker(Scope<AssetCooker> cooker)
    {
        mCookers[cooker->GetAssetType()] = std::move(cooker);
    }

    void AssetImporter::Shutdown() {}

    void AssetImporter::ScanAndRecookScripts()
    {
        Uint cooked = 0, skipped = 0;
        for(const auto& [id, meta] : mAssetManager->GetRegistryMap())
        {
            if(meta.Type != AssetType::SCRIPT)
                continue;

            if(!NeedsCook(id, meta.Type))
            {
                skipped++;
                continue;
            }

            const CookResult r = RecookAsset(id);
            if(r.Success)
                cooked++;
            else
                Log<Severity::Error>("[AssetImporter::ScanAndRecookScripts] Recook script failed: {}", meta.RelativePath);
        }
        Log<Severity::Info>("[AssetImporter::ScanAndRecookScripts]  Cooked: {} | Already up to date: {}", cooked, skipped);
    }

    void AssetImporter::ScanAndCookAll() const
    {
        Uint cooked = 0, skipped = 0;
        for(const auto& [id, meta] : mAssetManager->GetRegistryMap())
        {
            if(HasFlag(meta.Flags, AssetFlags::MEMORY) || meta.Type == AssetType::SCENE)
                continue;

            if(!NeedsCook(id, meta.Type))
            {
                skipped++;
                continue;
            }

            const CookResult r = Cook(id, meta, mAssetManager);
            if (r.Success)
                cooked++;
            else
                Log<Severity::Error>("[AssetImporter] Cook failed: '{}'", meta.RelativePath);
        }

        Log<Severity::Info>("[AssetImporter] Cooked: {} | Already up to date: {}", cooked, skipped);
    }

    CookResult AssetImporter::RecookAsset(AssetID id)
    {
        const AssetMetadata& meta = mAssetManager->GetMetadata(id);
        mAssetManager->Unload(id);
        return Cook(id, meta, mAssetManager);
    }

    bool AssetImporter::NeedsCook(AssetID id, AssetType type) const
    {
        auto it = mCookers.find(type);
        if(it == mCookers.end())
            return false;

        return it->second->NeedsCook(id);
    }

    Surge::CookResult AssetImporter::Cook(AssetID id, const AssetMetadata& meta, AssetManager* am) const
    {
        if(HasFlag(meta.Flags, AssetFlags::MEMORY))
            return {};

        auto it = mCookers.find(meta.Type);
        if(it == mCookers.end())
            return {};

        return it->second->Cook(am->GetAbsolutePath(meta.RelativePath), id);
    }

}