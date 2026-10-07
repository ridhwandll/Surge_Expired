// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Path.hpp"
#include "Surge/Core/Project.hpp"

namespace Surge
{
    class Scene;
    class Entity;
    namespace Serializer
    {
        void SerializeScene(const Path& path, Scene* in);
        void DeserializeScene(const Path& path, Scene* out);

        // In-memory single entity snapshot (JSON) of every serializable component except RelationshipComponent, used by the Editor (undo, copy/paste)
        String SerializeEntity(Entity entity);
        // Restores a SerializeEntity() snapshot onto 'entity': serializable components missing from the snapshot are removed.
        // IDComponent and RelationshipComponent are left untouched (hierarchy links are the caller's job)
        bool DeserializeEntity(const String& data, Entity entity);

        void SerializeProject(const Path& path, Project* in);
        void DeserializeProject(const Path& path, Project* out);
    }

} // namespace Surge::Serializer