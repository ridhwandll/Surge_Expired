// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Path.hpp"
#include "Surge/Core/String.hpp"

namespace Surge
{
    class Scene;
    class Entity;

    // JSON scene source (.srg), the Editor's authoring format. Every build loads the cooked scene (SceneFormat) instead
    namespace SceneSourceIO
    {
        String ToJson(Scene& scene);
        bool FromJson(const String& json, Scene& outScene);

        bool Write(Scene& scene, const Path& path);
        bool Read(const Path& path, Scene& outScene);

        // In-memory single entity snapshot (JSON) of every serializable component except RelationshipComponent (undo, copy/paste)
        String EntityToJson(Entity entity);
        // Restores an EntityToJson() snapshot onto 'entity': serializable components missing from the snapshot are removed.
        // IDComponent and RelationshipComponent are left untouched (hierarchy links are the caller's job)
        bool EntityFromJson(const String& json, Entity entity);
    }
}
