// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include <glm/glm.hpp>

namespace Surge
{
    struct Light
    {
        glm::vec4 PositionType;               // 16 bytes -> xyz = pos or dir, w = type (0 = dir, 1 = point)
        glm::vec3 Color;                      // 12 bytes -> rgb=color
        float Intensity;                      // 4 bytes -> light intensity
        float Radius;                         // 4 bytes  -> point light falloff
        float Falloff;                        // 4 bytes  -> point light falloff curve

        glm::vec2 _pad0;
    };
    static_assert(sizeof(Light) % 16 == 0, "Size of 'Light' struct must be 16 bytes aligned!");

    struct LightUBOData
    {
        Light Lights[256] = {};
    };
    static_assert(sizeof(LightUBOData) % 16 == 0, "Size of 'Lights' struct must be 16 bytes aligned!");

    enum class LightType : Uint
    {
        POINT = 0,
        DIRECTIONAL = 1
    };
    
} // namespace Surge
