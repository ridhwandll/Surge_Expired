// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "AssetBase.hpp"
#include <filesystem>

namespace Surge
{
    struct AssetMetadata
    {
        AssetHandle Handle = INVALID_ASSET_HANDLE;
        AssetType Type = AssetType::NONE;

        std::filesystem::path Path = ""; // This path must be relative to the Assets Directory
        bool IsDataLoaded = false;

        bool IsValid() { return IsAssetHandleValid(Handle) && Type != AssetType::NONE && Path != "" && IsDataLoaded != false; }
    };
}