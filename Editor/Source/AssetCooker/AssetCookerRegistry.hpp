// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "IAssetCooker.hpp"
#include "Surge/Asset/AssetManager.hpp"

namespace Surge
{
    class AssetCookerRegistry
    {
    public:
        void Register(Scope<IAssetCooker> cooker)
        {
            mCookers[cooker->GetAssetType()] = std::move(cooker);
        }

        CookResult Cook(AssetID id, AssetManager* am) const
        {
            const AssetMetadata& meta = am->GetMetadata(id);
            if(HasFlag(meta.Flags, AssetFlags::MEMORY))
                return {};

            auto it = mCookers.find(meta.Type);
            if(it == mCookers.end())
                return {};

            return it->second->Cook(am->GetAbsolutePath(meta.RelativePath));
        }

        void CookAll(AssetManager* am) const
        {
            Uint total = 0, cooked = 0, skipped = 0, failed = 0;

            for(const auto& [id, meta] : am->GetRegistryMap())
            {
                auto it = mCookers.find(meta.Type);
                if(it == mCookers.end())
                    continue;
                if(HasFlag(meta.Flags, AssetFlags::MEMORY))
                    continue;

                total++;
                const String absPath = am->GetAbsolutePath(meta.RelativePath);

                if(!it->second->NeedsCook(absPath))
                {
                    skipped++;
                    continue;
                }

                CookResult r = it->second->Cook(absPath);
                r.Success ? cooked++ : failed++;

                if(r.Success)
                    Log<Severity::Info>("[CookerRegistry] {} → {} ({} MB → {} MB)", meta.RelativePath, r.OutputPath, r.InputMegaBytes, r.OutputMegaBytes);
                else
                    Log<Severity::Error>("[CookerRegistry] Failed: {}", meta.RelativePath);
            }
            Log<Severity::Info>("[CookerRegistry] Total: {} | Cooked: {} | " "Skipped: {} | Failed: {}", total, cooked, skipped, failed);
        }

    private:
        std::unordered_map<AssetType, Scope<IAssetCooker>> mCookers;
    };
}
