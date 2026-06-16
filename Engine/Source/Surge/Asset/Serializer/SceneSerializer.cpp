// Copyright (c) - SurgeTechnologies - All rights reserved
#include "SceneSerializer.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Serializer/Serializer.hpp"
#include "Surge/Asset/AssetManager.hpp"

namespace Surge
{
    void SceneSerializer::Initialize()
    {
        mSerializerType = AssetType::SCENE;
    }

    bool SceneSerializer::Serialize(Ref<Asset> asset) const
    {
        bool result = false;
        AssetManager* assetManager = Core::GetAssetManager();
        const AssetMetadata& metadata = assetManager->GetMetadata(asset->GetID());

        String absolutePath = assetManager->GetAbsolutePath(metadata.RelativePath);
        Ref<Scene> scene = asset.As<Scene>();
        Serializer::SerializeScene(absolutePath, scene.Raw());

        return result;
    }

    Ref<Asset> SceneSerializer::Deserialize(const AssetMetadata& metadata) const
    {
        AssetManager* assetManager = Core::GetAssetManager();
        String absolutePath = assetManager->GetAbsolutePath(metadata.RelativePath);

        Ref<Scene> scene = Ref<Scene>::Create();
        Serializer::DeserializeScene(absolutePath, scene.Raw());
        return scene.As<Asset>();
    }

    void SceneSerializer::Shutdown()
    {
        Log<Severity::Info>("[SceneSerializer] Shutdown");
    }
}


