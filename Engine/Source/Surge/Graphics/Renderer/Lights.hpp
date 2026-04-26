// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include <glm/glm.hpp>
#include "Surge/Core/Defines.hpp"

namespace Surge
{
    struct PointLight
    {
        PointLight() = default;

        glm::vec3 Position = glm::vec3(0.0f, 0.0f, 0.0f);
        float Intensity = 1.0f;
        glm::vec3 Color = glm::vec3(1.0f, 1.0f, 1.0f);
        float Radius = 3.0f;
        float Falloff = 0.0f;

        glm::vec3 _Padding_ = {};
    };
    static_assert(sizeof(PointLight) % 16 == 0, "Size of 'PointLight' struct must be 16 bytes aligned!");

    struct DirectionalLight
    {
        glm::vec3 Direction;
        float Intensity;

        glm::vec3 Color;
        float Size;
    };
    static_assert(sizeof(DirectionalLight) % 16 == 0, "Size of 'DirectionalLight' struct must be 16 bytes aligned!");

    struct LightUniformBufferData
    {
        glm::vec3 CameraPosition = {};
        Uint PointLightCount = 0;

        PointLight PointLights[1024] = {};
        DirectionalLight DirLight = {};
    };
    static_assert(sizeof(LightUniformBufferData) % 16 == 0, "Size of 'Lights' struct must be 16 bytes aligned!");

} // namespace Surge
