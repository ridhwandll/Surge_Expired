// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Graphics/RHI/RHIHandle.hpp"
#include "Surge/Graphics/Material/MaterialRegistry.hpp"

namespace Surge
{
	class GraphicsRHI;
	struct GPUMaterial;
	class MaterialRegistry
	{
	public:
		static constexpr Uint MAX_MATERIALS = 1024;

		void Initialize(GraphicsRHI* rhi);
		void Shutdown();

		// Called by Material::Create, returns permanent slot index
		Uint Allocate();
		void Free(Uint slot);

		// Called by Material::Apply. writes GPU data into the buffer
		void Upload(Uint slot, const GPUMaterial& data);

		Uint GetBufferBindlessIndex() const { return mBufferIndex; }

	private:
		GraphicsRHI* mRHI = nullptr;
		BufferHandle mBuffer = BufferHandle::Invalid();
		Uint mBufferIndex = UINT32_MAX;

		Vector<Uint> mFreeSlots;
		Uint mNextSlot = 0;
	};
}