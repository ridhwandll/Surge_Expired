// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Path.hpp"
#include "Surge/Core/Defines.hpp"
#include "Surge/ECS/Scene.hpp"

namespace Surge::Serializer
{
    template <typename T>
    void Serialize(const Path& path, T* in)
    {
        static_assert(false);
    }
    template <typename T>
    void Deserialize(const Path& path, T* out)
    {
        static_assert(false);
    }

    template <>
    SURGE_API void Serialize(const Path& path, Scene* in);
    template <>
    SURGE_API void Deserialize(const Path& path, Scene* out);

} // namespace Surge::Serializer