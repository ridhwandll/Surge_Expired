// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "AssetCooker.hpp"
#include "Surge/Asset/AssetManager.hpp"

namespace Surge
{
    class AssetCookerRegistry
    {
    public:
        void Register(Scope<AssetCooker> cooker)
        {
            mCookers[cooker->GetAssetType()] = std::move(cooker);
        }

        bool NeedsCook(AssetID id, AssetType type) const
        {
            auto it = mCookers.find(type);
            if(it == mCookers.end())
                return false;

            return it->second->NeedsCook(id);
        }

        CookResult Cook(AssetID id, const AssetMetadata& meta, AssetManager* am) const
        {
            if(HasFlag(meta.Flags, AssetFlags::MEMORY))
                return {};

            auto it = mCookers.find(meta.Type);
            if(it == mCookers.end())
                return {};

            return it->second->Cook(am->GetAbsolutePath(meta.RelativePath), id);
        }

    private:
        std::unordered_map<AssetType, Scope<AssetCooker>> mCookers;
    };
}
