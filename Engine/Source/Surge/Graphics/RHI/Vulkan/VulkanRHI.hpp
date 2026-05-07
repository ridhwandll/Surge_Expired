// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Window/Window.hpp"
#include "Surge/Graphics/RHI/RHIHandle.hpp"
#include "Surge/Graphics/RHI/RHIDescriptors.hpp"
#include "Surge/Graphics/RHI/RHIFrameContext.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanDevice.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanDebugger.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanResourcePools.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanFrame.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanSwapchain.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanImGui.hpp"

#include <volk.h>
#include <vk_mem_alloc.h>



#define FORCE_VALIDATION 0
#if FORCE_VALIDATION == 1
	#define ENABLE_IF_VK_VALIDATION(x) { x; }
#else
	#ifdef SURGE_DEBUG
		#define ENABLE_IF_VK_VALIDATION(x) { x; }
	#else
		#define ENABLE_IF_VK_VALIDATION(x)
	#endif // SURGE_DEBUG
#endif

namespace Surge
{
	struct FrameContext;
	class VulkanRHI
	{
	public:
		VulkanRHI() = default;
		~VulkanRHI() = default;
		void Initialize(Window* window);
		void WaitIdle();
		void Shutdown();

		void BeginFrame(FrameContext& outCtx);
		void EndFrame(const FrameContext& ctx);
		void Resize();
		const RHIStats& GetStats();

		//Renderpass
		void CmdBeginSwapchainRenderpass(const FrameContext& ctx);
		void CmdEndSwapchainRenderpass(const FrameContext& ctx);

		void CmdBindVertexBuffer(const FrameContext& ctx, BufferHandle h, Uint offset = 0);
		void CmdBindIndexBuffer(const FrameContext& ctx, BufferHandle h, Uint offset = 0);

		BufferHandle CreateBuffer(const BufferDesc& desc);		
		void UploadBuffer(BufferHandle h, const void* data, Uint size, Uint offset);
		void DestroyBuffer(BufferHandle buffer);

		TextureHandle CreateTexture(const TextureDesc& desc, const void* initialData = nullptr);
		void DestroyTexture(TextureHandle texture);

		FramebufferHandle CreateFramebuffer(const FramebufferDesc& desc, const void* initialData = nullptr);
		void DestroyFramebuffer(FramebufferHandle framebuffer);

		// Pipeline
		PipelineHandle CreatePipeline(const PipelineDesc& desc);
		void DestroyPipeline(PipelineHandle h);

		//Draw Commands
		void CmdDrawIndexed(const FrameContext& ctx, Uint indexCount, Uint instanceCount, Uint firstIndex, int32_t vertexOffset, Uint firstInstance);
		void CmdDraw(const FrameContext& ctx, Uint vertexCount, Uint instanceCount, Uint firstVertex, Uint firstInstance);
		void CmdBindPipeline(const FrameContext& ctx, PipelineHandle h);
		void CmdPushConstants(const FrameContext& ctx, PipelineHandle h, const void* data, Uint size, Uint offset);

		void SetDebugName(const VkDebugUtilsObjectNameInfoEXT& nameInfo) const { mDebugger.SetDebugName(*this, nameInfo); }

		const VulkanSwapchain& GetSwapchain() const { return mSwapchain; }
		VulkanSwapchain& GetSwapchain() { return mSwapchain; }
		VulkanFrame& GetFrame() { return mFrame; }

		VkInstance GetInstance() const { return mInstance; }
		VkSurfaceKHR GetSurface() const { return mSurface; }
		VkFramebuffer GetSwapchainFramebuffer(Uint index) const { return mSwapchainFramebuffers[index]; }
		VkRenderPass GetRenderPass() const { return mRenderPass; }
		VkDevice GetDevice() const { return mDevice.GetDevice(); }
		VkPhysicalDevice GetGPU() const { return mDevice.GetGPU(); }
		VkQueue GetQueue() const { return mDevice.GetQueue(); }
		VmaAllocator GetAllocator() const { return mDevice.GetAllocator(); }
		int32_t GetQueueIndex() const { return mDevice.GetQueueIndex(); }

	private:
		void CreateInstance();
		void FillStats();

		void CreateSurface(Window* window);
		void DestroySurface();

		void CreateSwapchainFramebuffers();
		void DestroySwapchainFramebuffers();

		void CreateRenderpass();
		void DestroyRenderpass();

		void ResizeInternal();

		Vector<const char*> GetRequiredInstanceExtensions();
		Vector<const char*> GetRequiredInstanceLayers();

	private:
		RHIStats mStats;

		VulkanDevice mDevice;
		VulkanDebugger mDebugger;
		VulkanFrame mFrame;
		VulkanSwapchain mSwapchain;
		VulkanImGuiContext mImGuiContext;

		Vector<VkFramebuffer> mSwapchainFramebuffers;
		VkRenderPass mRenderPass = VK_NULL_HANDLE;

		VkInstance mInstance { VK_NULL_HANDLE };
		VkSurfaceKHR mSurface { VK_NULL_HANDLE };

		//Pools
		HandlePool<PipelineHandle, PipelineEntry> mPipelinePool;
		HandlePool<BufferHandle, BufferEntry> mBufferPool;
		HandlePool<FramebufferHandle, FramebufferEntry> mFramebufferPool;
	};

}