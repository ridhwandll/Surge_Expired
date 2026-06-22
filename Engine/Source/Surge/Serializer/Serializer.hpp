// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Path.hpp"
#include "Surge/Core/Project.hpp"

namespace Surge
{
    class Scene;
    namespace Serializer
    {
        void SerializeScene(const Path& path, Scene* in);
        void DeserializeScene(const Path& path, Scene* out);

        void SerializeProject(const Path& path, Project* in);
        void DeserializeProject(const Path& path, Project* out);
    }

} // namespace Surge::Serializer