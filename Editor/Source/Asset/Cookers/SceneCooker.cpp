// Copyright (c) - SurgeTechnologies - All rights reserved
#include "SceneCooker.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/ECS/Scene.hpp"
#include "Surge/Asset/Serializer/Scene/SceneSerializer.hpp"
#include "Surge/Utility/Filesystem.hpp"

namespace Surge
{
    CookResult SceneCooker::Cook(const String& sourceAbsPath, AssetID id) const
    {
        AssetManager* am = Core::GetAssetManager();
        const String sidecarPath = am->GetSidecarPath(id, AssetType::SCENE);

        CookResult result;
        result.OutputPath = sidecarPath;

        // Builds the scene like opening it does, which loads the assets it references
        String json;
        Ref<Scene> scene = Ref<Scene>::Create(Filesystem::GetFilenameWithoutExt(sourceAbsPath), Core::GetSceneContext());
        if(!Filesystem::ReadTextFile(sourceAbsPath, json) || !SceneSerializer::FromJson(json, *scene))
        {
            Log<Severity::Error>("[SceneCooker] Failed to read scene: {}", sourceAbsPath);
            return result;
        }

        result.Success = SceneBinary::Write(sidecarPath, AssetStampWriter::Build(sourceAbsPath, GetCookerVersion()), scene.Raw());
        result.InputMegaBytes = Filesystem::FileSizeInMB(sourceAbsPath);
        result.OutputMegaBytes = Filesystem::FileSizeInMB(sidecarPath);
        return result;
    }
}
