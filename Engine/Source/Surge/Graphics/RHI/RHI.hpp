// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Window/Window.hpp"
#include "Surge/Graphics/RHI/RHIHandle.hpp"
#include "Surge/Graphics/RHI/RHIDescriptors.hpp"

#if defined(SURGE_PLATFORM_WINDOWS) || defined(SURGE_PLATFORM_ANDROID)
#include "Surge/Graphics/RHI/Vulkan/VulkanRHI.hpp"
namespace Surge
{
	using BackendRHI = VulkanRHI;
}
#elif defined(SURGE_PLATFORM_APPLE)
// ERROR: This file doesn't exist yet, but will be implemented in the future when we add support for Apple platforms
#include "Surge/Graphics/RHI/Metal/MetalRHI.hpp"
namespace Surge
{
	using BackendRHI = MetalRHI;
}
#endif


namespace Surge
{
	class GraphicsRHI
	{
	public:
		GraphicsRHI() = default;
		~GraphicsRHI() = default;

		void Initialize(Window* window = nullptr) { mBackendRHI.Initialize(window); }
		void Shutdown() { mBackendRHI.Shutdown(); }

		// Buffer
		BufferHandle CreateBuffer(const BufferDesc& desc) { return mBackendRHI.CreateBuffer(desc); }
		void UploadBuffer(BufferHandle h, const void* data, Uint size, Uint offset) { mBackendRHI.UploadBuffer(h, data, size, offset); }
		void DestroyBuffer(BufferHandle buffer) { mBackendRHI.DestroyBuffer(buffer); }

		// Texture
		TextureHandle CreateTexture(const TextureDesc& desc, const void* initialData = nullptr) { return mBackendRHI.CreateTexture(desc, initialData); }
		void DestroyTexture(TextureHandle texture) { mBackendRHI.DestroyTexture(texture); }

		// Framebuffer
		FramebufferHandle CreateFramebuffer(const FramebufferDesc& desc, const void* initialData = nullptr) { return mBackendRHI.CreateFramebuffer(desc, initialData); }
		void DestroyFramebuffer(FramebufferHandle framebuffer) { mBackendRHI.DestroyFramebuffer(framebuffer); }


		// TODO: REMOVE
		BackendRHI& GetBackendRHI() { return mBackendRHI; }

	private:
		BackendRHI mBackendRHI;
	};
}
