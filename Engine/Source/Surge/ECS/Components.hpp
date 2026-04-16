// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include "Surge/Core/UUID.hpp"
#include "Surge/Graphics/Material.hpp"
#include "Surge/Graphics/Mesh.hpp"
#include "Surge/Graphics/Camera/RuntimeCamera.hpp"
#include "SurgeReflect/SurgeReflect.hpp"
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Surge
{
    struct SURGE_API IDComponent
    {
        IDComponent() = default;
        IDComponent(const UUID& id)
            : ID(id) {}

        UUID ID;

        SURGE_REFLECTION_ENABLE;
    };

    struct SURGE_API NameComponent
    {
        NameComponent() = default;
        NameComponent(const String& name)
            : Name(name) {}

        String Name;

        SURGE_REFLECTION_ENABLE;
    };

    struct SURGE_API TransformComponent
    {
        TransformComponent() = default;
        TransformComponent(const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& scale)
            : Position(position), Rotation(rotation), Scale(scale) {}

        glm::vec3 Position = glm::vec3(0.0f, 0.0f, 0.0f);
        glm::vec3 Rotation = glm::vec3(0.0f, 0.0f, 0.0f);
        glm::vec3 Scale = glm::vec3(1.0f, 1.0f, 1.0f);

        glm::mat4 GetTransform()
        {
            const glm::mat4 rot = glm::toMat4(glm::quat(glm::radians(Rotation)));
            glm::mat4 result = glm::translate(glm::mat4(1.0f), Position) * rot * glm::scale(glm::mat4(1.0f), Scale);
            return result;
        }

        SURGE_REFLECTION_ENABLE;
    };

    struct SURGE_API MeshComponent
    {
        MeshComponent() = default;
        MeshComponent(const Ref<Mesh>& mesh)
            : Mesh(mesh) {}

        Ref<Surge::Mesh> Mesh;

        SURGE_REFLECTION_ENABLE;
    };

    struct SURGE_API CameraComponent
    {
        CameraComponent() = default;
        CameraComponent(const RuntimeCamera& cam, bool primary, bool fixedAspectRatio)
            : Camera(cam), Primary(primary), FixedAspectRatio(fixedAspectRatio) {}

        RuntimeCamera Camera;
        bool Primary = true;
        bool FixedAspectRatio = false;

        SURGE_REFLECTION_ENABLE;
    };

    struct SURGE_API PointLightComponent
    {
        PointLightComponent() = default;
        PointLightComponent(glm::vec3 color, float intensity, float radius, float falloff)
            : Color(color), Intensity(intensity), Radius(radius), Falloff(falloff) {}

        glm::vec3 Color = {1.0f, 1.0f, 1.0f};
        float Intensity = 1.0f;
        float Radius = 3.0f;
        float Falloff = 0.0f;
        SURGE_REFLECTION_ENABLE;
    };

    struct SURGE_API DirectionalLightComponent
    {
        DirectionalLightComponent() = default;
        DirectionalLightComponent(glm::vec3 direction, glm::vec3 color, float intensity)
            : Direction(direction), Color(color), Intensity(intensity) {}

        glm::vec3 Direction = {1.0f, 1.0f, 1.0f};
        glm::vec3 Color = {1.0f, 1.0f, 1.0f};
        float Intensity = 1.0f;
        float Size = 45.5f;

        SURGE_REFLECTION_ENABLE;
    };

//! NOTE: ALL THE MAJOR COMPONENTS MUST BE REGISTERED HERE, ADD BY SEPARATING VIA A COMMA (',') WHEN YOU ADD A NEW COMPONENT
#define ALL_MAJOR_COMPONENTS ::Surge::IDComponent, ::Surge::NameComponent, ::Surge::TransformComponent,      \
                             ::Surge::MeshComponent, ::Surge::CameraComponent, ::Surge::PointLightComponent, \
                             ::Surge::DirectionalLightComponent

} // namespace Surge