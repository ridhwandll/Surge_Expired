// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/ECS/SceneContext.hpp"

namespace Surge
{
    // What the serializers and cooked readers work with, set by Core (AssetManager::Initialize), handed to every serializer
    struct AssetContext
    {
        AssetManager* AssetManager_ = nullptr; // Dependencies (a material's textures, a mesh's material overrides) load through it
        Renderer* Renderer_ = nullptr;         // GPU resources of textures, meshes, fonts and materials
        SceneContext Scenes;                   // Handed to the scenes it loads
    };

} // namespace Surge
