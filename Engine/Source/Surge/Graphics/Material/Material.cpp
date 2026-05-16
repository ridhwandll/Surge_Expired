// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Material.hpp"
#include "Surge/Graphics/RHI/RHI.hpp"
#include "Surge/Core/Core.hpp"

namespace Surge
{
	Material::Material(MaterialRegistry& registry, const String& debugName /*= "DefaultMat"*/)
	{
		mRegistry = &registry;
		mSlot = registry.Allocate();
		mDirty = true;
		Apply(); // upload defaults immediately
	}

	Material::~Material()
	{
		if (mSlot != UINT32_MAX)
			mRegistry->Free(mSlot);
	}

	Material& Material::SetAlbedo(const glm::vec3& color)
	{
		mGPUData.AlbedoMetallic = glm::vec4(color, mGPUData.AlbedoMetallic.w);
		mDirty = true;
		return *this;
	}

	Material& Material::SetMetallic(float v)
	{
		mGPUData.AlbedoMetallic.w = v;
		mDirty = true;
		return *this;
	}    

	Material& Material::SetRoughness(float v)
	{
		mGPUData.Roughness = v;
		mDirty = true;
		return *this;
	}

	Material& Material::SetReflectance(float v)
	{
		mGPUData.Reflectance = v;
		mDirty = true;
		return *this;
	}

	Material& Material::SetAlbedoTexture(TextureHandle h)
	{
		Uint whiteTextureIndex = Core::GetRenderer()->GetWhiteTextureBindlessIndex();
		const Scope<GraphicsRHI>& rhi = Core::GetRenderer()->GetRHI();

		mGPUData.AlbedoTexIndex = h.IsNull() ? whiteTextureIndex : rhi->GetBindlessTextureIndex(h);
		mDirty = true;
		return *this;
	}

	Material& Material::SetNormalTexture(TextureHandle h)
	{
		Uint whiteTextureIndex = Core::GetRenderer()->GetWhiteTextureBindlessIndex();
		const Scope<GraphicsRHI>& rhi = Core::GetRenderer()->GetRHI();

		mGPUData.NormalTexIndex = h.IsNull() ? whiteTextureIndex : rhi->GetBindlessTextureIndex(h);
		mDirty = true;
		return *this;
	}

	void Material::Apply()
	{
		if (!mDirty)
			return;

		mRegistry->Upload(mSlot, mGPUData);
		mDirty = false;
	}
}