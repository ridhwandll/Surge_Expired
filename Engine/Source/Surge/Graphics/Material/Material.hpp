// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/Memory.hpp"
#include <glm/glm.hpp>
#include "Surge/Graphics/RHI/RHIHandle.hpp"
#include "Surge/Graphics/Material/MaterialRegistry.hpp"

namespace Surge
{
	struct GPUMaterial
	{
		glm::vec4 AlbedoMetallic = { 1.0f, 1.0f, 1.0f, 0.0f }; // xyz=albedo, w=metallic
		float Roughness = 0.5f;
		float Reflectance = 0.5f;
		Uint AlbedoTexIndex = 0;    // 0 = white texture in bindless array
		Uint NormalTexIndex = 0;    // 0 = flat normal
	};
	static_assert(sizeof(GPUMaterial) == 32, "GPUMaterial size must match GLSL std430");

	class GraphicsRHI;
	class Material : public RefCounted
	{
	public:
		Material(MaterialRegistry& registry, const String& debugName);
		~Material();

		Material& SetAlbedo(const glm::vec3& color);
		Material& SetMetallic(float v);
		Material& SetRoughness(float v);
		Material& SetReflectance(float v);
		Material& SetAlbedoTexture(TextureHandle h);
		Material& SetNormalTexture(TextureHandle h);

		glm::vec3 GetAlbedo() const { return glm::vec3(mGPUData.AlbedoMetallic); }
		float GetMetallic() const { return mGPUData.AlbedoMetallic.w; }
		float GetRoughness() const { return mGPUData.Roughness; }
		float GetReflectance() const { return mGPUData.Reflectance; }

		// Uploads pending GPU data to the registry buffer
		void Apply();

		Uint GetMaterialIndex() const { return mSlot; }

	private:
		MaterialRegistry* mRegistry = nullptr;
		Uint mSlot = UINT32_MAX;
		GPUMaterial mGPUData = {};
		bool mDirty = false;
	};

}