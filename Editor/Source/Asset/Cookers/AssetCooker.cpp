// Copyright (c) - SurgeTechnologies - All rights reserved
#include "AssetCooker.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Surge/Asset/Serializer/AssetStamp.hpp"

namespace Surge
{
    bool AssetCooker::NeedsCook(AssetID id) const
    {
        AssetManager* am = Core::GetAssetManager();
        const AssetMetadata& meta = am->GetMetadata(id);
        const String sidecarPath = am->GetSidecarPath(id);

        if (HasFlag(AssetFlags::MEMORY, meta.Flags))
            return false;

        // No sidecar, must cook
        if(!Filesystem::Exists(sidecarPath))
            return true;

        // Bad stamp, must cook
        AssetStamp stamp;
        if(!AssetStampWriter::Read(sidecarPath, stamp))
            return true;

        const String sourceAbsPath = am->GetAbsolutePath(meta.RelativePath);
        bool needsCook = !AssetStampWriter::IsUpToDate(stamp, sourceAbsPath, GetCookerVersion());

        if(needsCook)
            Log<Severity::Debug>("[AssetCooker] Asset {} needs cook, source modified!", meta.RelativePath);

        return needsCook;
    }
}