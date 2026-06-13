// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include <glm/ext/vector_float3.hpp>

namespace Surge
{
    struct AABB
    {
    public:
        AABB() = default;
        AABB(const glm::vec3& min, const glm::vec3& max) : Min(min), Max(max) {}

        ~AABB() = default;

        void Reset()
        {
            Min = {FLT_MAX, FLT_MAX, FLT_MAX};
            Max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
        }

        glm::vec3 Min;
        glm::vec3 Max;
    };
} // namespace Surge
