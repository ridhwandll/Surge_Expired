// Copyright (c) - SurgeTechnologies - All rights reserved
#include "MaterialSerializer.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Asset/Serializer/Material/MaterialBinaryFormat.hpp"

namespace Surge
{
    void MaterialSerializer::Initialize()
    {
        mSerializerType = AssetType::MATERIAL;
    }

    bool MaterialSerializer::Serialize(Ref<Asset> asset) const
    {
#ifdef SURGE_PLATFORM_ANDROID
        Log<Severity::Error>("[MaterialSerializer] Serialization is unsupported on Android runtime. Pre-cook the assets!");
        return false;
#else
        AssetManager* am = Core::GetAssetManager();
        const AssetMetadata& meta = am->GetMetadata(asset->GetID());

        Ref<Material> material = asset.As<Material>();
        const String sidecarPath = am->GetSidecarPath(meta.ID);

        bool result = MaterialBinary::Write(sidecarPath, material);
        return result;
#endif
    }

    Ref<Asset> MaterialSerializer::Deserialize(const AssetMetadata& metadata) const
    {
        AssetManager* am = Core::GetAssetManager();
        const String sidecarPath = am->GetSidecarPath(metadata.ID);

        Ref<Material> material;
        MaterialBinary::Read(sidecarPath, material);
        return material.As<Asset>();
    }

    void MaterialSerializer::Shutdown()
    {
        Log<Severity::Info>("[MaterialSerializer] Shutdown");
    }
}
