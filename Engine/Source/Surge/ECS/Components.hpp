// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include "Surge/Core/UUID.hpp"
#include "Surge/Graphics/Camera/RuntimeCamera.hpp"
#include "Surge/Graphics/RHI/RHIHandle.hpp"
#include "Surge/Graphics/Mesh/Mesh.hpp"
#include "SurgeReflect/SurgeReflect.hpp"
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Surge
{
    struct IDComponent
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

		glm::vec3 Position = glm::vec3(0.0f);
		glm::vec3 Rotation = glm::vec3(0.0f); // Degrees
		glm::vec3 Scale = glm::vec3(1.0f);

		// Call transform changed (e.g. after physics)
		void MarkDirty() { mDirty = true; }

		const glm::mat4& GetTransform() const
		{
			if (mDirty)
			{
				mCachedTransform = glm::translate(glm::mat4(1.0f), Position)
					* glm::mat4_cast(glm::quat(glm::radians(Rotation)))
					* glm::scale(glm::mat4(1.0f), Scale);

				mDirty = false;
			}
			return mCachedTransform;
		}

		SURGE_REFLECTION_ENABLE;

	private:
		mutable glm::mat4 mCachedTransform{ 1.0f };
		mutable bool mDirty = true;
	};


	struct SpriteRendererComponent
	{
        SpriteRendererComponent() = default;
		SpriteRendererComponent(const glm::vec4& color)
			: Color(color), Texture(TextureHandle::Invalid()) {}
		SpriteRendererComponent(const glm::vec3& color, float alpha)
			: Color(glm::vec4(color, alpha)), Texture(TextureHandle::Invalid()) {}
		SpriteRendererComponent(const glm::vec4& colorTint, TextureHandle texture)
			: Color(colorTint), Texture(texture) {}

        glm::vec4 Color;
		TextureHandle Texture = TextureHandle::Invalid();

		SURGE_REFLECTION_ENABLE;
	};

    struct CameraComponent
    {
        CameraComponent() = default;
        CameraComponent(const RuntimeCamera& cam, bool primary, bool fixedAspectRatio)
            : Camera(cam), Primary(primary), FixedAspectRatio(fixedAspectRatio) {}

        RuntimeCamera Camera;
        bool Primary = true;
        bool FixedAspectRatio = false;

        SURGE_REFLECTION_ENABLE;
    };

    struct MeshComponent
    {
        Ref<Mesh> Mesh;
        SURGE_REFLECTION_ENABLE;
	};

    struct PointLightComponent
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
#define SERIALIZABLE_COMPONENTS ::Surge::IDComponent, ::Surge::NameComponent, ::Surge::TransformComponent,      \
                             ::Surge::CameraComponent, ::Surge::PointLightComponent, ::Surge::SpriteRendererComponent,\
                             ::Surge::DirectionalLightComponent

} // namespace Surge