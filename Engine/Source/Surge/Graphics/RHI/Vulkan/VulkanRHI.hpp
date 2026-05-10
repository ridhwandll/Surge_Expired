// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Window/Window.hpp"
#include "Surge/Graphics/RHI/RHIHandle.hpp"
#include "Surge/Graphics/RHI/RHIDescs.hpp"
#include "Surge/Graphics/RHI/RHIFrameContext.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanDevice.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanDebugger.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanResourcePools.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanFrame.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanSwapchain.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanFramebuffer.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanImGui.hpp"


#include <volk.h>
#include <vk_mem_alloc.h>
#include "VulkanBindlessRegistry.hpp"



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

		BufferHandle CreateBuffer(const BufferDesc& desc);		
		void UploadBuffer(BufferHandle h, const void* data, Uint size, Uint offset);
		void DestroyBuffer(BufferHandle buffer);

		TextureHandle CreateTexture(const TextureDesc& desc, const void* initialData = nullptr);
		void DestroyTexture(TextureHandle h);
		void UploadTextureData(TextureHandle h, const void* data, Uint size);
		void ResizeTexture(TextureHandle h, Uint width, Uint height);

		FramebufferHandle CreateFramebuffer(const FramebufferDesc& desc);
		void DestroyFramebuffer(FramebufferHandle h);
		void ResizeFramebuffer(FramebufferHandle h, Uint width, Uint height);

		PipelineHandle CreatePipeline(const PipelineDesc& desc);
		void DestroyPipeline(PipelineHandle h);

		SamplerHandle CreateSampler(const SamplerDesc& desc);
		void DestroySampler(SamplerHandle h);

		DescriptorLayoutHandle CreateDescriptorLayout(const DescriptorLayoutDesc& desc);
		DescriptorLayoutHandle GetDescriptorLayout(PipelineHandle h) const;
		void DestroyDescriptorLayout(DescriptorLayoutHandle h);

		Uint GetBindlessIndex(TextureHandle h);
		void BindBindlessSet(const FrameContext& ctx, PipelineHandle pipeline);
		DescriptorSetHandle CreateDescriptorSet(DescriptorLayoutHandle layoutHandle, DescriptorUpdateFrequency frequency, const char* debugName = nullptr);
		void UpdateDescriptorSet(DescriptorSetHandle setHandle, const DescriptorWrite* writes, Uint writeCount);
		void DestroyDescriptorSet(DescriptorSetHandle h);

		// Commands
		void CmdDrawIndexed(const FrameContext& ctx, Uint indexCount, Uint instanceCount, Uint firstIndex, int32_t vertexOffset, Uint firstInstance);
		void CmdDraw(const FrameContext& ctx, Uint vertexCount, Uint instanceCount, Uint firstVertex, Uint firstInstance);

		void CmdBindVertexBuffer(const FrameContext& ctx, BufferHandle h, Uint offset = 0);
		void CmdBindIndexBuffer(const FrameContext& ctx, BufferHandle h, Uint offset = 0);
		void CmdBindPipeline(const FrameContext& ctx, PipelineHandle h);

		void CmdPushConstants(const FrameContext& ctx, PipelineHandle h, ShaderType shaderStage, Uint offset, Uint size, const void* data);
		void CmdBlitToSwapchain(const FrameContext& ctx, TextureHandle srcHandle);
		void CmdTextureBarrier(const FrameContext& ctx, TextureHandle h, VkImageLayout newLayout); // TODO: Expose to RHI

		void CmdBeginSwapchainRenderpass(const FrameContext& ctx);
		void CmdEndSwapchainRenderpass(const FrameContext& ctx);

		void CmdBeginRenderPass(const FrameContext& ctx, FramebufferHandle h, glm::vec4 clearColor);
		void CmdEndRenderPass(const FrameContext& ctx);

		// setIndex maps to layout(set = N) in GLSL
		void CmdBindDescriptorSet(const FrameContext& ctx, PipelineHandle pipeline, DescriptorSetHandle setHandle, Uint setIndex);

		VkCommandBuffer BeginOneTimeCommands();
		void EndOneTimeCommands(VkCommandBuffer cmd);

		void SetDebugName(const VkDebugUtilsObjectNameInfoEXT& nameInfo) const { mDebugger.SetDebugName(*this, nameInfo); }

		// Getters for internal use by Vulkan* classes
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

		void ShowPoolDebugImGuiWindow();
	private:
		void CreateInstance();
		void FillStats();

		void CreateSurface(Window* window);
		void DestroySurface();

		void CreateSwapchainFramebuffers();
		void DestroySwapchainFramebuffers();

		void CreateSwapchainRenderpass();
		void DestroySwapchainRenderpass();

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
		VulkanRenderpassFactory mRenderPassCache;
		VulkanBindlessRegistry mBindlessRegistry;

		Vector<VkFramebuffer> mSwapchainFramebuffers;
		VkRenderPass mRenderPass = VK_NULL_HANDLE;

		VkInstance mInstance { VK_NULL_HANDLE };
		VkSurfaceKHR mSurface { VK_NULL_HANDLE };

		//Pools
		HandlePool<PipelineHandle, PipelineEntry> mPipelinePool;
		HandlePool<BufferHandle, BufferEntry> mBufferPool;
		HandlePool<FramebufferHandle, FramebufferEntry> mFramebufferPool;
		HandlePool<TextureHandle, TextureEntry> mTexturePool;
		HandlePool<SamplerHandle, SamplerEntry> mSamplerPool;
		HandlePool<DescriptorLayoutHandle, DescriptorLayoutEntry> mDescriptorLayoutPool;
		HandlePool<DescriptorSetHandle, DescriptorSetEntry> mDescriptorSetPool;

		friend class VulkanPipeline;
		friend class VulkanTexture;
	};

}