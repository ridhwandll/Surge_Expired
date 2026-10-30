// Copyright (c) - SurgeTechnologies - All rights reserved
#include "SceneCooker.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/ECS/Scene.hpp"
#include "Surge/Asset/Format/BinarySerialization.hpp"
#include "Asset/SourceIO/SceneSourceIO.hpp"

namespace Surge
{
    bool SceneCooker::Cook(const CookContext& context) const
    {
        // Builds the scene like opening it does, which loads the assets it references
        Ref<Scene> scene = Ref<Scene>::Create(context.SourcePath.stem().string(), Core::GetSceneContext());
        if(!SceneSourceIO::Read(context.SourcePath, *scene))
        {
            Log<Severity::Error>("[SceneCooker] Failed to read scene: {}", context.SourcePath.string());
            return false;
        }

        Rapt0R::Writer payload(64 * 1024);
        SceneBinary::Write(payload, scene.Raw());
        return context.Write(SceneBinary::VERSION, payload);
    }
}
