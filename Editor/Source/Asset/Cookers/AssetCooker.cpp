// Copyright (c) - SurgeTechnologies - All rights reserved
#include "AssetCooker.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Utility/Filesystem.hpp"

namespace Surge
{
    bool AssetCooker::NeedsCook(AssetID id) const
    {
        AssetManager* am = Core::GetAssetManager();
        return !Filesystem::Exists(am->GetSidecarPath(id));
    }
}