// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Asset/Asset.hpp"
#include "Surge/Core/String.hpp"

namespace Surge
{
    struct CookResult
    {
        bool Success = false;
        String OutputPath;
        float InputMegaBytes = 0;
        float OutputMegaBytes = 0;
    };

    class AssetCooker
    {
    public:
        virtual ~AssetCooker() = default;

        virtual CookResult Cook(const String& sourceAbsPath, AssetID id) const = 0;
        virtual AssetType GetAssetType() const = 0;

        bool NeedsCook(AssetID id) const;
    };
}