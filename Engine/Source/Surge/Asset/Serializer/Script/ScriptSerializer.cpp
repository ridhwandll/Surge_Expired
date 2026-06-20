// Copyright (c) - SurgeTechnologies - All rights reserved
#include "ScriptSerializer.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Asset/Serializer/AssetStamp.hpp"
#include "Surge/Asset/Serializer/Script/ScriptBinaryFormat.hpp"
#include "Surge/ScriptEngine/ScriptAsset.hpp"
#include <Surge/Asset/Serializer/BinaryHelpers.hpp>

namespace Surge
{
    void ScriptSerializer::Initialize()
    {
        mSerializerType = AssetType::SCRIPT;
    }

    bool ScriptSerializer::Serialize(Ref<Asset> asset) const
    {
#ifdef SURGE_PLATFORM_ANDROID
        Log<Severity::Error>("[ScriptSerializer] Serialization is unsupported on Android runtime. Pre-cook the assets!");
        return false;
#else
        AssetManager* am = Core::GetAssetManager();

        SCOPED_TIMER("ScriptSerializer::Serialize");
        Ref<Script> script = asset.As<Script>();
        const String sidecarPath = am->GetSidecarPath(asset->GetID());

        AssetStamp existingStamp;
        AssetStampWriter::Read(sidecarPath, existingStamp);

        return ScriptBinary::Write(sidecarPath, existingStamp, script);
#endif
    }

    Ref<Asset> ScriptSerializer::Deserialize(const AssetMetadata& metadata) const
    {
        AssetManager* am = Core::GetAssetManager();
        const String sidecarPath = am->GetSidecarPath(metadata.ID);

        AssetStamp stamp;
        Ref<Script> script = nullptr;
        if (!ScriptBinary::Read(sidecarPath, stamp, script))
            Log<Severity::Error>("[ScriptSerializer] Failed to read script bytecode for asset ID: {}. Path: {}", metadata.ID.Get(), sidecarPath);

        return script;
    }

    void ScriptSerializer::Shutdown()
    {
    }
}


