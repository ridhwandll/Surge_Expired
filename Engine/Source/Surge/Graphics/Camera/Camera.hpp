// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include <glm/glm.hpp>

namespace Surge
{
    enum class CameraMode
    {
        None,
        Flycam,
        Arcball
    };

    class Camera
    {
    public:
        Camera() = default;
        Camera(const glm::mat4& projection) : mProjection(projection) {}
        virtual ~Camera() = default;

        const glm::mat4& GetProjectionMatrix() const { return mProjection; }

    protected:
        glm::mat4 mProjection = glm::mat4(1.0f);
    };
} // namespace Surge
