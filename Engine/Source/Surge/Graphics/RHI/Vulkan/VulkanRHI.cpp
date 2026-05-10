// Copyright (c) - SurgeTechnologies - All rights reserved
#include "VulkanRHI.hpp"
#include "VulkanDebugger.hpp"
#include "VulkanBuffer.hpp"
#include "VulkanPipeline.hpp"
#include "VulkanUtils.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Core/Logger/Logger.hpp"


#ifdef SURGE_PLATFORM_ANDROID
#include <android/native_window.h>
#endif
#include "VulkanTexture.hpp"
#include "Backends/imgui_impl_vulkan.h"

namespace Surge
{
	static bool ValidateExtensions(const Vector<const char*>& required, const Vector<VkExtensionProperties>& available)
	{
		for (auto extension : required)
		{
			bool found = false;
			for (auto& availableExtension : available)
			{
				if (strcmp(availableExtension.extensionName, extension) == 0)
				{
					found = true;
					break;
				}
			}

			if (!found)
			{
				return false;
			}
		}

		return true;
	}

	static String GetVendorName(Uint vendorID)
	{
		switch (vendorID) {
		case 0x10DE: return "NVIDIA";
		case 0x1002: return "AMD";
		case 0x8086: return "INTEL";
		case 0x13B5: return "ARM (Mali)";
		case 0x5143: return "Qualcomm (Adreno)";
		case 0x1010: return "ImgTec (PowerVR)";
		default:     return "Unknown Vendor";
		}
	}

	void VulkanRHI::Initialize(Window* window)
	{
		CreateInstance();
		CreateSurface(window);
		mDevice.Initialize(mInstance, mSurface);
		mFrame.Initialize(*this, GraphicsRHI::FRAMES_IN_FLIGHT);
		mSwapchain.Initialize(*this, window->GetSize().x, window->GetSize().y);

		CreateSwapchainRenderpass();
		CreateSwapchainFramebuffers();

		mImGuiContext.Init(*this);
		FillStats();
		mBindlessRegistry.Init(*this);
	}

	void VulkanRHI::WaitIdle()
	{
		vkDeviceWaitIdle(mDevice.GetDevice());
	}

	void VulkanRHI::Shutdown()
	{	
		mBindlessRegistry.Shutdown(*this);
		mRenderPassCache.Shutdown(*this);
		mImGuiContext.Shutdown(*this);

		//mDescriptorSetPool.ForEachAlive([&](const DescriptorSetHandle& h, DescriptorSetEntry& entry) { DestroyDescriptorSet(h); SG_ASSERT_INTERNAL("You forgot to destroy a descriptor layout manually!"); });
		//mDescriptorLayoutPool.ForEachAlive([&](const DescriptorLayoutHandle& h, DescriptorLayoutEntry& entry) { DestroyDescriptorLayout(h); SG_ASSERT_INTERNAL("You forgot to destroy a descriptor layout manually!"); });
		//mSamplerPool.ForEachAlive([&](const SamplerHandle& h, SamplerEntry& entry){ DestroySampler(h); SG_ASSERT_INTERNAL("You forgot to destroy a sampler manually!"); });
		//mFramebufferPool.ForEachAlive([&](const FramebufferHandle& h, FramebufferEntry& entry) { VulkanFramebuffer::Destroy(*this, entry); SG_ASSERT_INTERNAL("You forgot to destroy a framebuffer manually!"); });
		//mTexturePool.ForEachAlive([&](const TextureHandle& h, TextureEntry& entry) { VulkanTexture::Destroy(*this, entry); SG_ASSERT_INTERNAL("You forgot to destroy a texture manually!"); });
		//mBufferPool.ForEachAlive([&](const BufferHandle& h, BufferEntry& entry) { VulkanBuffer::Destroy(*this, entry); SG_ASSERT_INTERNAL("You forgot to destroy a buffer manually!"); });
		//mPipelinePool.ForEachAlive([&](const PipelineHandle& h, PipelineEntry& entry) { VulkanPipeline::Destroy(*this, entry); SG_ASSERT_INTERNAL("You forgot to destroy a pipeline manually!"); });

		DestroySwapchainFramebuffers();
		DestroySwapchainRenderpass();

		mFrame.Shutdown(*this);
		mSwapchain.Shutdown(*this);
		ENABLE_IF_VK_VALIDATION(mDebugger.EndDiagnostics(mInstance));
		mDevice.Destroy();
		vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
		vkDestroyInstance(mInstance, nullptr);
	}

	void VulkanRHI::BeginFrame(FrameContext& outCtx)
	{
		mStats.Reset();
		mImGuiContext.BeginFrame();
		VkDevice device = mDevice.GetDevice();

		// Get current frame SLOT (round-robin, predictable)
		PerFrame& frame = mFrame.GetCurrentVkFrame();
		Uint swapchainWidth = mSwapchain.GetWidth();
		Uint swapchainHeight = mSwapchain.GetHeight();

		// Wait for this SLOT's fence
		// This slot was used N frames ago, wait until the GPU is done with it
		vkWaitForFences(device, 1, &frame.Fence, VK_TRUE, UINT64_MAX);


		// Ask swapchain which IMAGE is available
		// This is unpredictable, diver may return any index
		// AcquireSemaphore signals when the image is actually ready to write
		Uint imageIndex = 0;
		VkResult result = mSwapchain.AcquireNextImage(*this, frame.AcquireSemaphore, imageIndex);

		outCtx.FrameIndex = mFrame.GetCurrentFrameIndex();
		outCtx.SwapchainIndex = imageIndex;
		outCtx.Width = swapchainWidth;
		outCtx.Height = swapchainHeight;

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
		{
			// Handle resize 
			// IS this good? Or I should get the window size from Core?
			ResizeInternal();
			result = mSwapchain.AcquireNextImage(*this, frame.AcquireSemaphore, imageIndex);
			outCtx.Width = mSwapchain.GetWidth();
			outCtx.Height = mSwapchain.GetHeight();
		}
		SG_ASSERT(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR, "VulkanRHI: AcquireNextImage failed");

		vkResetFences(device, 1, &frame.Fence);
		vkResetCommandPool(device, frame.CmdPool, 0);

		// Begin recording
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(frame.CmdBuffer, &beginInfo);

	}

	void VulkanRHI::EndFrame(const FrameContext& ctx)
	{
		PerFrame& frame = mFrame.GetCurrentVkFrame();
		VkSemaphore releaseSemaphore = mSwapchain.GetFrame(ctx.SwapchainIndex).ReleaseSemaphore;

		vkEndCommandBuffer(frame.CmdBuffer);

		// Submit: frame slot semaphores sync with swapchain image
		// Wait on AcquireSemaphore -> GPU waits until compositor releases the image
		// Signal ReleaseSemaphore -> tells compositor GPU is done writing to it
		// Signal Fence -> tells CPU this slot is free N frames from now
		VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &frame.AcquireSemaphore;
		submitInfo.pWaitDstStageMask = &waitStage;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &frame.CmdBuffer;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &releaseSemaphore;
		vkQueueSubmit(mDevice.GetQueue(), 1, &submitInfo, frame.Fence);

		// Present: swapchain image index + release semaphore from slot
		mSwapchain.Present(*this, releaseSemaphore, ctx.SwapchainIndex);

		// Advance SLOT index (completely independent of swapchain image index)
		mFrame.AdvanceFrame();
	}

	void VulkanRHI::Resize()
	{
#ifdef SURGE_PLATFORM_ANDROID
        //VulkanRHI& rhi = Core::GetRenderer()->GetRHI()->GetBackendRHI();
		//vkDestroySurfaceKHR(rhi.GetInstance(), rhi.GetSurface(), nullptr);
		//CreateSurface(Core::GetWindow());
#endif		
		Core::AddFrameEndCallback([this]() {
				ResizeInternal();
			});
	}

	const RHIStats& VulkanRHI::GetStats()
	{
		// We need to write the memory stats every time RHI::GetStats() is called because GPU memory can change dynamically
		const GPUMemoryStats& memStats = mDevice.GetMemoryStats();
		uint64_t bytes = memStats.AllocatedBytes.load(std::memory_order_relaxed);
		mStats.UsedGPUMemory = (float)bytes / (1024.0f * 1024.0f);
		mStats.TotalAllowedGPUMemory = (float)memStats.BudgetBytes / (1024.0f * 1024.0f);
		mStats.AllocationCount = memStats.AllocationCount.load(std::memory_order_relaxed);

		return mStats;
	}

	BufferHandle VulkanRHI::CreateBuffer(const BufferDesc& desc)
	{
		Log<Severity::Trace>("VulkanRHI::CreateBuffer: Name: {0} Size: {1} bytes", desc.DebugName ? desc.DebugName : "Unnamed", desc.Size);
		BufferEntry entry = VulkanBuffer::Create(*this, desc);
		return mBufferPool.Allocate(std::move(entry));
	}

	void VulkanRHI::UploadBuffer(BufferHandle h, const void* data, Uint size, Uint offset)
	{
		BufferEntry* entry = mBufferPool.Get(h);
		SG_ASSERT(entry != nullptr, "UploadBuffer: invalid handle!");

		VulkanBuffer::Upload(*this, *entry, data, size, offset);
	}

	void VulkanRHI::DestroyBuffer(BufferHandle buffer)
	{
		BufferEntry* entry = mBufferPool.Get(buffer);
		if (!entry)		
			return;
		
		Log<Severity::Info>("VulkanRHI::DestroyBuffer: Size: {0} bytes", entry->Desc.Size);
		VulkanBuffer::Destroy(*this, *entry);// kills VkBuffer + VmaAllocation
		mBufferPool.Free(buffer); // Return slot to free list
	}

	TextureHandle VulkanRHI::CreateTexture(const TextureDesc& desc, const void* initialData /*= nullptr*/)
	{		
		// TRANSFER_DST is required when InitialData is provided
		SG_ASSERT(!(desc.InitialData && !(desc.Usage & TextureUsage::TRANSFER_DST)), "TextureDesc: InitialData provided but TRANSFER_DST not set in Usage. " "Add TextureUsage::TRANSFER_DST to upload pixel data.");

		TextureEntry entry = VulkanTexture::Create(*this, desc);
		TextureHandle h = mTexturePool.Allocate(std::move(entry));

		SamplerEntry* samplerEntry = mSamplerPool.Get(desc.Sampler);
		SG_ASSERT(samplerEntry, "Null sampler, please provide a valid Sampler Handle in TextureDesc");

		{
			mTexturePool.Get(h)->BindlessIndex = mBindlessRegistry.RegisterTexture(*this, entry.View, samplerEntry->Sampler);
		}

		if (desc.InitialData && desc.DataSize > 0)
			UploadTextureData(h, desc.InitialData, desc.DataSize);

		Log<Severity::Trace>("VulkanRHI::CreateTexture of size {0}x{1} with format {2} and usage {3}", desc.Width, desc.Height, static_cast<Uint>(desc.Format), static_cast<Uint>(desc.Usage));
		return h;
	}

	void VulkanRHI::DestroyTexture(TextureHandle h)
	{		
		TextureEntry* entry = mTexturePool.Get(h);
		if (!entry)
			return;

		Log<Severity::Info>("Destroying texture with handle index {0} and generation {1}", h.Index, h.Generation);
		mBindlessRegistry.UnregisterTexture(entry->BindlessIndex);
		VulkanTexture::Destroy(*this, *entry);
		mTexturePool.Free(h);		
	}

	void VulkanRHI::UploadTextureData(TextureHandle h, const void* data, Uint size)
	{
		SG_ASSERT(data && size > 0, "UploadTextureData: data is null or size is 0");
		VulkanTexture::UploadData(*this, h, data, size);
	}

	void VulkanRHI::ResizeTexture(TextureHandle h, Uint width, Uint height)
	{
		vkDeviceWaitIdle(mDevice.GetDevice());

		TextureEntry* entry = mTexturePool.Get(h);
		if (!entry)
			return;

		VulkanTexture::Destroy(*this, *entry);
		TextureDesc desc = entry->Desc;
		desc.Width = width;
		desc.Height = height;

		*entry = VulkanTexture::Create(*this, desc);
	}

	FramebufferHandle VulkanRHI::CreateFramebuffer(const FramebufferDesc& desc)
	{
		Log<Severity::Trace>("VulkanRHI::CreateFramebuffer of size {0}x{1} with {2} color attachments and depth attachment: {3}", desc.Width, desc.Height, desc.ColorAttachmentCount, desc.HasDepth);
		FramebufferEntry entry = VulkanFramebuffer::Create(*this, desc, mRenderPassCache, mTexturePool);
		return mFramebufferPool.Allocate(std::move(entry));
	}

	void VulkanRHI::DestroyFramebuffer(FramebufferHandle h)
	{
		FramebufferEntry* entry = mFramebufferPool.Get(h);
		if (!entry)
			return;

		Log<Severity::Info>("Destroying framebuffer with handle index {0} and generation {1}", h.Index, h.Generation);
		VulkanFramebuffer::Destroy(*this, *entry);
		mFramebufferPool.Free(h);		
	}

	void VulkanRHI::ResizeFramebuffer(FramebufferHandle h, Uint width, Uint height)
	{
		vkDeviceWaitIdle(mDevice.GetDevice());

		FramebufferEntry* entry = mFramebufferPool.Get(h);
		if (!entry)
			return;

		// Destroy old VkFramebuffer, textures still live
		VulkanFramebuffer::Destroy(*this, *entry);

		// Rebuild with new dimensions
		// Caller must resize textures first via ResizeTexture()
		FramebufferDesc desc = entry->Desc;
		desc.Width = width;
		desc.Height = height;

		*entry = VulkanFramebuffer::Create(*this, desc, mRenderPassCache, mTexturePool);
	}

	PipelineHandle VulkanRHI::CreatePipeline(const PipelineDesc& desc)
	{
		SG_ASSERT(!desc.TargetFramebuffer.IsNull() || desc.TargetSwapchain, "PipelineDesc: must set either TargetFramebuffer or TargetSwapchain");
		SG_ASSERT(!((!desc.TargetFramebuffer.IsNull()) && desc.TargetSwapchain), "PipelineDesc: cannot set both TargetFramebuffer and TargetSwapchain");

		//Log<Severity::Trace>("\nVulkanRHI::CreatePipeline:\n   Name: {0}\n   Vertex Shader: {1}\n   Fragment Shader: {2}", desc.DebugName ? desc.DebugName : "Unnamed", desc.VertShaderName, desc.FragShaderName);

		VkRenderPass renderPass = VK_NULL_HANDLE;

		if (!desc.TargetFramebuffer.IsNull())
		{
			FramebufferEntry* fb = mFramebufferPool.Get(desc.TargetFramebuffer);
			SG_ASSERT(fb, "CreatePipeline: invalid TargetFramebuffer handle");
			renderPass = fb->RenderPass; // Borrowed from cache
		}
		else if (desc.TargetSwapchain)		
			renderPass = mRenderPass;
		
		SG_ASSERT(renderPass != VK_NULL_HANDLE, "CreatePipeline: failed to resolve render pass");
		PipelineEntry entry = VulkanPipeline::Create(*this, desc, renderPass);
		return mPipelinePool.Allocate(std::move(entry));
	}

	void VulkanRHI::DestroyPipeline(PipelineHandle h)
	{
		PipelineEntry* entry = mPipelinePool.Get(h);
		if (!entry)
			return;

		Log<Severity::Info>("Destroying pipeline with handle index {0} and generation {1}", h.Index, h.Generation);
		VulkanPipeline::Destroy(*this, *entry);
		mPipelinePool.Free(h);
	}

	SamplerHandle VulkanRHI::CreateSampler(const SamplerDesc& desc)
	{
		VkSamplerCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		info.magFilter = VulkanUtils::ToVkFilter(desc.Mag);
		info.minFilter = VulkanUtils::ToVkFilter(desc.Min);
		info.mipmapMode = desc.Mip == MipmapMode::LINEAR ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
		info.addressModeU = VulkanUtils::ToVkWrap(desc.WrapU);
		info.addressModeV = VulkanUtils::ToVkWrap(desc.WrapV);
		info.addressModeW = VulkanUtils::ToVkWrap(desc.WrapU);
		info.mipLodBias = desc.MipBias;
		info.maxLod = VK_LOD_CLAMP_NONE;
		info.anisotropyEnable = desc.Anisotropy ? VK_TRUE : VK_FALSE;
		info.maxAnisotropy = desc.MaxAniso;

		SamplerEntry entry = {};
		entry.Desc = desc;

		VK_CALL(vkCreateSampler(mDevice, &info, nullptr, &entry.Sampler));
		return mSamplerPool.Allocate(std::move(entry));
	}

	void VulkanRHI::DestroySampler(SamplerHandle h)
	{
		Log<Severity::Info>("Destroying sampler with handle index {0} and generation {1}", h.Index, h.Generation);
		SamplerEntry* entry = mSamplerPool.Get(h);
		if (!entry)
			return;

		vkDestroySampler(mDevice, entry->Sampler, nullptr);
		mSamplerPool.Free(h);
	}

	DescriptorLayoutHandle VulkanRHI::CreateDescriptorLayout(const DescriptorLayoutDesc& desc)
	{
		DescriptorLayoutEntry entry = {};
		entry.Desc = desc;

		VkDescriptorSetLayoutBinding vkBindings[16] = {};
		VkDescriptorBindingFlags vkBindingFlags[16] = {};

		for (Uint i = 0; i < desc.BindingCount; i++)
		{
			const DescriptorBinding& b = desc.Bindings[i];
			vkBindings[i].binding = b.Slot;
			vkBindings[i].descriptorType = VulkanUtils::ToVkDescriptorType(b.Type);
			vkBindings[i].descriptorCount = b.Count;
			vkBindings[i].stageFlags = VulkanUtils::ShaderTypeToVulkanShaderStage(b.Stage);

			// Bindless
			VkDescriptorBindingFlags flags =
				VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |         
				VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |       
				VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
		}

		VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo = {};
		flagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
		flagsInfo.bindingCount = desc.BindingCount;
		flagsInfo.pBindingFlags = vkBindingFlags;

		VkDescriptorSetLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.pNext = &flagsInfo;
		layoutInfo.bindingCount = desc.BindingCount;
		layoutInfo.pBindings = vkBindings;

		VK_CALL(vkCreateDescriptorSetLayout(mDevice, &layoutInfo, nullptr, &entry.Layout));
		return mDescriptorLayoutPool.Allocate(std::move(entry));
	}

	DescriptorLayoutHandle VulkanRHI::GetDescriptorLayout(PipelineHandle h) const
	{
		const PipelineEntry* entry = mPipelinePool.Get(h);
		SG_ASSERT(entry, "GetDescriptorLayout: invalid pipeline handle");
		return entry->DescriptorSetLayout;
	}

	void VulkanRHI::DestroyDescriptorLayout(DescriptorLayoutHandle h)
	{
		Log<Severity::Info>("Destroying descriptor layout with handle index {0} and generation {1}", h.Index, h.Generation);
		DescriptorLayoutEntry* entry = mDescriptorLayoutPool.Get(h);
		if (!entry)
			return;

		vkDestroyDescriptorSetLayout(mDevice, entry->Layout, nullptr);
		mDescriptorLayoutPool.Free(h);
	}

	Surge::Uint VulkanRHI::GetBindlessIndex(TextureHandle h)
	{
		TextureEntry* entry = mTexturePool.Get(h);
		SG_ASSERT(entry, "GetBindlessIndex: invalid TextureHandle");

		return entry->BindlessIndex;
	}

	void VulkanRHI::BindBindlessSet(const FrameContext& ctx, PipelineHandle pipeline)
	{
		VkCommandBuffer cmd = mFrame.GetFrame(ctx.FrameIndex).CmdBuffer;

		PipelineEntry* entry = mPipelinePool.Get(pipeline);
		SG_ASSERT(entry, "BindBindlessSet: invalid PipelineHandle");

		mBindlessRegistry.Bind(cmd, entry->Layout);
	}

	DescriptorSetHandle VulkanRHI::CreateDescriptorSet(DescriptorLayoutHandle layoutHandle, DescriptorUpdateFrequency frequency, const char* debugName /*= nullptr*/)
	{
		DescriptorLayoutEntry* layout = mDescriptorLayoutPool.Get(layoutHandle);
		SG_ASSERT(layout, "CreateDescriptorSet: invalid DescriptorLayoutHandle");

		DescriptorSetEntry entry = {};
		entry.Layout = layoutHandle;
		entry.Frequency = frequency;
		entry.Count = (frequency == DescriptorUpdateFrequency::DYNAMIC) ? GraphicsRHI::FRAMES_IN_FLIGHT : 1;

		// Build pool sizes from cached layout bindings
		// Pool must hold entry.Count sets worth of descriptors
		std::unordered_map<VkDescriptorType, Uint> typeCounts;
		for (Uint i = 0; i < layout->Desc.BindingCount; i++)
		{
			VkDescriptorType vkType = VulkanUtils:: ToVkDescriptorType(layout->Desc.Bindings[i].Type);
			typeCounts[vkType] += layout->Desc.Bindings[i].Count * entry.Count;
		}

		Vector<VkDescriptorPoolSize> poolSizes;
		poolSizes.reserve(typeCounts.size());
		for (auto& [type, count] : typeCounts)
			poolSizes.push_back({ type, count });

		VkDescriptorPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.maxSets = entry.Count;
		poolInfo.poolSizeCount = (Uint)poolSizes.size();
		poolInfo.pPoolSizes = poolSizes.data();

		VK_CALL(vkCreateDescriptorPool(mDevice, &poolInfo, nullptr, &entry.Pool));

		// Allocate entry.Count sets from the pool
		VkDescriptorSetLayout layouts[GraphicsRHI::FRAMES_IN_FLIGHT] = {};
		for (Uint i = 0; i < entry.Count; i++)
			layouts[i] = layout->Layout;

		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = entry.Pool;
		allocInfo.descriptorSetCount = entry.Count;
		allocInfo.pSetLayouts = layouts;

		VK_CALL(vkAllocateDescriptorSets(mDevice, &allocInfo, entry.Sets));

#if defined(SURGE_DEBUG)
		if (debugName)
		{
			for (Uint i = 0; i < entry.Count; i++)
			{
				String name = String(debugName) + (entry.Count > 1 ? " [Frame " + std::to_string(i) + "]" : "");

				VkDebugUtilsObjectNameInfoEXT nameInfo = {};
				nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
				nameInfo.objectType = VK_OBJECT_TYPE_DESCRIPTOR_SET;
				nameInfo.objectHandle = (uint64_t)entry.Sets[i];
				nameInfo.pObjectName = name.c_str();
				SetDebugName(nameInfo);
			}
		}
#endif

		return mDescriptorSetPool.Allocate(std::move(entry));

	}

	void VulkanRHI::UpdateDescriptorSet(DescriptorSetHandle setHandle, const DescriptorWrite* writes, Uint writeCount)
	{
		DescriptorSetEntry* entry = mDescriptorSetPool.Get(setHandle);
		SG_ASSERT(entry, "UpdateDescriptorSet: invalid DescriptorSetHandle");

		// Dynamic: write into current frame's copy
		// Static: write into the single copy
		VkDescriptorSet targetSet = (entry->Frequency == DescriptorUpdateFrequency::DYNAMIC) ? entry->Sets[mFrame.GetCurrentFrameIndex()] : entry->Sets[0];

		Vector<VkDescriptorImageInfo>  imageInfos;
		Vector<VkDescriptorBufferInfo> bufferInfos;
		Vector<VkWriteDescriptorSet>   vkWrites;
		imageInfos.reserve(writeCount);
		bufferInfos.reserve(writeCount);
		vkWrites.reserve(writeCount);

		for (Uint i = 0; i < writeCount; i++)
		{
			const DescriptorWrite& w = writes[i];

			VkWriteDescriptorSet vkWrite = {};
			vkWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			vkWrite.dstSet = targetSet;
			vkWrite.dstBinding = w.Slot;
			vkWrite.dstArrayElement = w.ArrayIndex;
			vkWrite.descriptorCount = 1;
			vkWrite.descriptorType = VulkanUtils::ToVkDescriptorType(w.Type);

			switch (w.Type)
			{
			case DescriptorType::TEXTURE:
			case DescriptorType::STORAGE_TEXTURE:
			{
				TextureEntry* tex = mTexturePool.Get(w.Texture);
				SG_ASSERT(tex, "UpdateDescriptorSet: invalid TextureHandle at slot");

				SamplerEntry* smp = mSamplerPool.Get(w.Sampler);

				VkDescriptorImageInfo img = {};
				img.sampler = smp ? smp->Sampler : VK_NULL_HANDLE;
				img.imageView = tex->View;
				img.imageLayout = (w.Type == DescriptorType::STORAGE_TEXTURE) ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				imageInfos.push_back(img);
				vkWrite.pImageInfo = &imageInfos.back();
				break;
			}
			case DescriptorType::UNIFORM_BUFFER:
			case DescriptorType::STORAGE_BUFFER:
			{
				BufferEntry* buf = mBufferPool.Get(w.Buffer);
				SG_ASSERT(buf, "UpdateDescriptorSet: invalid BufferHandle at slot");

				VkDescriptorBufferInfo bufInfo = {};
				bufInfo.buffer = buf->Buffer;
				bufInfo.offset = w.BufferOffset;
				bufInfo.range = (w.BufferRange == 0) ? buf->Desc.Size : w.BufferRange;
				bufferInfos.push_back(bufInfo);
				vkWrite.pBufferInfo = &bufferInfos.back();
				break;
			}
			default:
				SG_ASSERT(false, "UpdateDescriptorSet: unhandled DescriptorType");
				break;
			}
			vkWrites.push_back(vkWrite);
		}

		vkUpdateDescriptorSets(mDevice, (Uint)vkWrites.size(), vkWrites.data(), 0, nullptr);
	}

	void VulkanRHI::DestroyDescriptorSet(DescriptorSetHandle h)
	{
		Log<Severity::Info>("Destroying descriptor set with handle index {0} and generation {1}", h.Index, h.Generation);
		DescriptorSetEntry* entry = mDescriptorSetPool.Get(h);
		if (!entry)
			return;

		// Pool destruction implicitly frees all allocated sets
		if (entry->Pool != VK_NULL_HANDLE)
		{
			vkDestroyDescriptorPool(mDevice, entry->Pool, nullptr);
			entry->Pool = VK_NULL_HANDLE;
		}

		for (auto& s : entry->Sets)
			s = VK_NULL_HANDLE;

		mDescriptorSetPool.Free(h);
	}

	void VulkanRHI::CmdDrawIndexed(const FrameContext& ctx, Uint indexCount, Uint instanceCount, Uint firstIndex, int32_t vertexOffset, Uint firstInstance)
	{
		VkCommandBuffer cmd = mFrame.GetFrame(ctx.FrameIndex).CmdBuffer;
		vkCmdDrawIndexed(cmd, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
		mStats.DrawCalls++;
	}

	void VulkanRHI::CmdDraw(const FrameContext& ctx, Uint vertexCount, Uint instanceCount, Uint firstVertex, Uint firstInstance)
	{
		VkCommandBuffer cmd = mFrame.GetFrame(ctx.FrameIndex).CmdBuffer;
		vkCmdDraw(cmd, vertexCount, instanceCount, firstVertex, firstInstance);
		mStats.DrawCalls++;
	}

	void VulkanRHI::CmdBindVertexBuffer(const FrameContext& ctx, BufferHandle h, Uint offset /*= 0*/)
	{
		VkCommandBuffer cmd = mFrame.GetFrame(ctx.FrameIndex).CmdBuffer;
		const BufferEntry* entry = mBufferPool.Get(h);
		SG_ASSERT(entry != nullptr, "CmdBindVertexBuffer: Invalid BufferHandle");
		SG_ASSERT(entry->Buffer != VK_NULL_HANDLE, "CmdBindVertexBuffer: null VkBuffer");

		VkDeviceSize vkOffset = offset;
		vkCmdBindVertexBuffers(cmd, 0, 1, &entry->Buffer, &vkOffset);
	}

	void VulkanRHI::CmdBindIndexBuffer(const FrameContext& ctx, BufferHandle h, Uint offset /*= 0*/)
	{
		VkCommandBuffer cmd = mFrame.GetFrame(ctx.FrameIndex).CmdBuffer;
		const BufferEntry* entry = mBufferPool.Get(h);
		SG_ASSERT(entry != nullptr, "CmdBindIndexBuffer: Invalid BufferHandle");
		vkCmdBindIndexBuffer(cmd, entry->Buffer, offset, VK_INDEX_TYPE_UINT32);
	}

	void VulkanRHI::CmdBindPipeline(const FrameContext& ctx, PipelineHandle h)
	{
		VkCommandBuffer cmd = mFrame.GetFrame(ctx.FrameIndex).CmdBuffer;
		PipelineEntry* entry = mPipelinePool.Get(h);
		SG_ASSERT(entry, "CmdBindPipeline: invalid handle");
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, entry->Pipeline);
	}

	void VulkanRHI::CmdPushConstants(const FrameContext& ctx, PipelineHandle h, ShaderType shaderStage, Uint offset, Uint size, const void* data)
	{
		VkCommandBuffer cmd = mFrame.GetFrame(ctx.FrameIndex).CmdBuffer;
		PipelineEntry* entry = mPipelinePool.Get(h);
		SG_ASSERT(entry, "CmdPushConstants: invalid handle");
		vkCmdPushConstants(cmd, entry->Layout, VulkanUtils::ShaderTypeToVulkanShaderStage(shaderStage), offset, size, data);
	}

	void VulkanRHI::CmdBlitToSwapchain(const FrameContext& ctx, TextureHandle srcHandle)
	{
		VkCommandBuffer cmd = mFrame.GetFrame(ctx.FrameIndex).CmdBuffer;
		TextureEntry* src = mTexturePool.Get(srcHandle);
		SG_ASSERT(src, "CmdBlitToSwapchain: invalid source TextureHandle");

		VkImage swapchainImage = mSwapchain.GetFrame(ctx.SwapchainIndex).Image;
		Uint w = mSwapchain.GetDimensions().Width;
		Uint h = mSwapchain.GetDimensions().Height;

		// Transition offscreen: COLOR_ATTACHMENT_OPTIMAL to TRANSFER_SRC
		VkImageMemoryBarrier srcBarrier = {};
		srcBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		srcBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		srcBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		srcBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		srcBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		srcBarrier.image = src->Image;
		srcBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		srcBarrier.subresourceRange.levelCount = 1;
		srcBarrier.subresourceRange.layerCount = 1;

		// Transition swapchain: UNDEFINED to TRANSFER_DST
		VkImageMemoryBarrier dstBarrier = {};
		dstBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		dstBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		dstBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		dstBarrier.srcAccessMask = 0;
		dstBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		dstBarrier.image = swapchainImage;
		dstBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		dstBarrier.subresourceRange.levelCount = 1;
		dstBarrier.subresourceRange.layerCount = 1;

		VkImageMemoryBarrier preBlit[2] = { srcBarrier, dstBarrier };
		vkCmdPipelineBarrier(cmd,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			0, 0, nullptr, 0, nullptr,
			2, preBlit);

		// Blit
		VkImageBlit region = {};
		region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.srcSubresource.layerCount = 1;
		region.srcOffsets[0] = { 0, 0, 0 };
		region.srcOffsets[1] = { (int32_t)w, (int32_t)h, 1 };
		region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.dstSubresource.layerCount = 1;
		region.dstOffsets[0] = { 0, 0, 0 };
		region.dstOffsets[1] = { (int32_t)w, (int32_t)h, 1 };

		vkCmdBlitImage(cmd, src->Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_NEAREST);

		// Transition swapchain: TRANSFER_DST to COLOR_ATTACHMENT
		// ImGui pass will render on top (needs COLOR_ATTACHMENT_OPTIMAL)
		VkImageMemoryBarrier postBlit = {};
		postBlit.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		postBlit.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		postBlit.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		postBlit.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		postBlit.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
		postBlit.image = swapchainImage;
		postBlit.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		postBlit.subresourceRange.levelCount = 1;
		postBlit.subresourceRange.layerCount = 1;

		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &postBlit);

		// Track layout for offscreen texture
		src->Layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	}

	void VulkanRHI::CmdTextureBarrier(const FrameContext& ctx, TextureHandle h, VkImageLayout newLayout)
	{
		TextureEntry* entry = mTexturePool.Get(h);
		SG_ASSERT(entry, "VulkanRHI::CmdTextureBarrier: invalid TextureHandle");

		VkCommandBuffer cmd = mFrame.GetFrame(ctx.FrameIndex).CmdBuffer;
		VulkanTexture::TransitionLayout(cmd, *entry, newLayout);
	}

	void VulkanRHI::CmdBeginSwapchainRenderpass(const FrameContext& ctx)
	{
		// Set clear color values
		VkClearValue clearValue{ .color = {{0.1f, 0.1f, 0.1f, 1.0f}} };

		VkRenderPassBeginInfo rpbegin{
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.renderPass = mRenderPass,
			.framebuffer = mSwapchainFramebuffers[ctx.SwapchainIndex],
			.renderArea = {.extent = {.width = ctx.Width, .height = ctx.Height}},
			.clearValueCount = 1,
			.pClearValues = &clearValue };

		VkCommandBuffer cmd = mFrame.GetCurrentVkFrame().CmdBuffer;
		vkCmdBeginRenderPass(cmd, &rpbegin, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport vp{
			.width = static_cast<float>(ctx.Width),
			.height = static_cast<float>(ctx.Height),
			.minDepth = 0.0f,
			.maxDepth = 1.0f };
		vkCmdSetViewport(cmd, 0, 1, &vp); // Set viewport dynamically
		VkRect2D scissor{ .extent = {.width = ctx.Width, .height = ctx.Height} };
		vkCmdSetScissor(cmd, 0, 1, &scissor); // Set scissor dynamically
	}

	void VulkanRHI::CmdEndSwapchainRenderpass(const FrameContext& ctx)
	{
		VkCommandBuffer cmd = mFrame.GetFrame(ctx.FrameIndex).CmdBuffer;
		mImGuiContext.EndFrame(cmd);
		vkCmdEndRenderPass(cmd);
	}

	void VulkanRHI::CmdBeginRenderPass(const FrameContext& ctx, FramebufferHandle h, glm::vec4 clearColor)
	{
		FramebufferEntry* entry = mFramebufferPool.Get(h);
		SG_ASSERT(entry, "CmdBeginRenderPass: invalid FramebufferHandle");

		VkCommandBuffer cmd = mFrame.GetFrame(ctx.FrameIndex).CmdBuffer;

		if (entry->ClearCount > 0)
			entry->ClearValues[0].color = { {clearColor.r, clearColor.g, clearColor.b, clearColor.a} };

		VkRenderPassBeginInfo rpInfo = {};
		rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		rpInfo.renderPass = entry->RenderPass;
		rpInfo.framebuffer = entry->Framebuffer;
		rpInfo.renderArea.extent = { entry->Desc.Width, entry->Desc.Height };
		rpInfo.clearValueCount = entry->ClearCount;
		rpInfo.pClearValues = entry->ClearValues.data();
		vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport vp = {};
		vp.width = (float)entry->Desc.Width;
		vp.height = (float)entry->Desc.Height;
		vp.minDepth = 0.0f;
		vp.maxDepth = 1.0f;
		vkCmdSetViewport(cmd, 0, 1, &vp);

		VkRect2D scissor = {};
		scissor.extent = { entry->Desc.Width, entry->Desc.Height };
		vkCmdSetScissor(cmd, 0, 1, &scissor);
	}

	void VulkanRHI::CmdEndRenderPass(const FrameContext& ctx)
	{
		VkCommandBuffer cmd = mFrame.GetFrame(ctx.FrameIndex).CmdBuffer;
		vkCmdEndRenderPass(cmd);
	}

	void VulkanRHI::CmdBindDescriptorSet(const FrameContext& ctx, PipelineHandle pipeline, DescriptorSetHandle setHandle, Uint setIndex /*= 0*/)
	{
		DescriptorSetEntry* set = mDescriptorSetPool.Get(setHandle);
		PipelineEntry* pipe = mPipelinePool.Get(pipeline);
		VkCommandBuffer     cmd = mFrame.GetFrame(ctx.FrameIndex).CmdBuffer;
		SG_ASSERT(set && pipe, "CmdBindDescriptorSet: invalid handle");

		// Dynamic: bind this frame's copy, Static: bind the only copy
		VkDescriptorSet targetSet = (set->Frequency == DescriptorUpdateFrequency::DYNAMIC) ? set->Sets[ctx.FrameIndex] : set->Sets[0];
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe->Layout, setIndex, 1, &targetSet, 0, nullptr);
	}

	VkCommandBuffer VulkanRHI::BeginOneTimeCommands()
	{
		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = mFrame.GetFrame(0).CmdPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer cmd;
		vkAllocateCommandBuffers(mDevice, &allocInfo, &cmd);

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(cmd, &beginInfo);
		return cmd;
	}

	void VulkanRHI::EndOneTimeCommands(VkCommandBuffer cmd)
	{
		vkEndCommandBuffer(cmd);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmd;

		vkQueueSubmit(mDevice.GetQueue(), 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(mDevice.GetQueue());
		vkFreeCommandBuffers(mDevice.GetDevice(), mFrame.GetFrame(0).CmdPool, 1, &cmd);
	}

	void VulkanRHI::ShowPoolDebugImGuiWindow()
	{
		ImGui::Begin("Vulkan RHI");
		constexpr float boldFontSize = 18.0f;
		ImGui::PushFont(mImGuiContext.GetBoldFont(), 22.0f);
		ImGui::Text("GPU Info");
		ImGui::PopFont();		
		ImGui::Text("GPU: %s", mStats.GPUName.c_str());
		ImGui::Text("Vendor: %s", mStats.VendorName.c_str());
		ImGui::Text("%s", mStats.RHIVersion.c_str());
		ImGui::Text("Draw call(s): %i", mStats.DrawCalls);

		// We need to call GetStats() here to update the memory stats before displaying them
		auto& memStats = GetStats();
		float used = memStats.UsedGPUMemory;
		float total = memStats.TotalAllowedGPUMemory;
		float frac = (total > 0.0f) ? (used / total) : 0.0f;
		ImGui::PushFont(mImGuiContext.GetBoldFont(), boldFontSize);
		ImGui::Text("GPU Memory");
		ImGui::PopFont();
		ImGui::Text("Allocations: %llu", memStats.AllocationCount);
		ImGui::Text("%.3f MB / %.3f MB", used, total);
		ImGui::ProgressBar(frac, ImVec2(-1, 0));
		ImGui::Separator();
		ImGui::PushFont(mImGuiContext.GetBoldFont(), 22.0f);
		ImGui::Text("RHI Pools");
		ImGui::PopFont();

		ImGui::PushFont(mImGuiContext.GetBoldFont(), boldFontSize);
		ImGui::Text("BufferPool");
		ImGui::PopFont();
		ImGui::Text("Alive objects: %d", mBufferPool.AliveObjCount());
		mBufferPool.ForEachAlive([](const BufferHandle& h, BufferEntry& entry)
		{
			String bufText = std::format("BufferHandle ({}, {})", h.Index, h.Generation);
			if (ImGui::TreeNode(bufText.c_str()))
			{
				ImGui::Text("Debug Name: %s", entry.Desc.DebugName);
				ImGui::Text("Size: %.3f MB", entry.Desc.Size / (1024.0f * 1024.0f));
				ImGui::Text("Usage: %s", VulkanUtils::BufferUsageToString(entry.Desc.Usage));
				ImGui::Text("Host Visible: %s", entry.Desc.HostVisible ? "Yes" : "No");
				ImGui::TreePop();
			}
		});

		ImGui::Separator();
		ImGui::PushFont(mImGuiContext.GetBoldFont(), boldFontSize);
		ImGui::Text("PipelinePool");
		ImGui::PopFont();
		ImGui::Text("Alive objects: %d", mPipelinePool.AliveObjCount());
		mPipelinePool.ForEachAlive([](const PipelineHandle& h, PipelineEntry& entry)
			{
				String pipeText = std::format("PipelineHandle ({}, {})", h.Index, h.Generation);
				if (ImGui::TreeNode(pipeText.c_str()))
				{
					ImGui::Text("Debug Name: %s", entry.Desc.DebugName);
					ImGui::Text("Target: %s", entry.Desc.TargetSwapchain ? "Swapchain" : "Framebuffer");
					ImGui::Text("Shader: %s", entry.Desc.Shader_.GetName().c_str());
					ImGui::TreePop();
				}
			});

		ImGui::Separator();
		ImGui::PushFont(mImGuiContext.GetBoldFont(), boldFontSize);
		ImGui::Text("FramebufferPool");
		ImGui::PopFont();
		ImGui::Text("Alive objects: %d", mFramebufferPool.AliveObjCount());
		mFramebufferPool.ForEachAlive([this](const FramebufferHandle& h, FramebufferEntry& entry)
			{
				FramebufferDesc desc = entry.Desc;
				String fbufText = std::format("FramebufferHandle ({}, {})", h.Index, h.Generation);
				if (ImGui::TreeNode(fbufText.c_str()))
				{
					ImGui::Text("Debug Name: %s", entry.Desc.DebugName);
					ImGui::Text("Dimensions: %dx%d", desc.Width, desc.Height);
					ImGui::Text("Color Attachment Count: %d", desc.ColorAttachmentCount);
					if (desc.ColorAttachmentCount > 0)
					{
						if (ImGui::TreeNode("ColorAttachments"))
						{
							for (Uint i = 0; i < desc.ColorAttachmentCount; i++)
							{
								TextureEntry* texEntry = mTexturePool.Get(desc.ColorAttachments[i].Handle);
								TextureDesc tDesc = texEntry->Desc;
								String texText = std::format("TextureHandle ({}, {})", h.Index, h.Generation);
								if (ImGui::TreeNode(texText.c_str()))
								{
									ImGui::Text("Debug Name: %s", tDesc.DebugName.c_str());
									ImGui::Text("Dimensions: %dx%d", tDesc.Width, tDesc.Height);
									ImGui::Text("Format: %s", VulkanUtils::TextureFormatToString(tDesc.Format).c_str());
									ImGui::Text("Usage: %s", VulkanUtils::TextureUsageToString(tDesc.Usage));
									ImGui::Text("Size: %.3f MB", texEntry->Size / (1024.0f * 1024.0f));
									ImGui::TreePop();
								}
							}
							ImGui::TreePop();
						}
					}

					ImGui::Text("Has Depth: %s", desc.HasDepth ? "Yes" : "No");
					if (desc.HasDepth)
					{
						ImGui::Text("Depth Attachment:");
						ImGui::Text("TextureHandle (%d, %d), LoadOp: %s, StoreOp: %s", desc.DepthAttachment.Handle.Index, desc.DepthAttachment.Handle.Generation,
							VulkanUtils::LoadOpToString(desc.DepthAttachment.Load), VulkanUtils::StoreOpToString(desc.DepthAttachment.Store));
					}
					ImGui::TreePop();
				}
			});

		ImGui::Separator();
		ImGui::PushFont(mImGuiContext.GetBoldFont(), boldFontSize);
		ImGui::Text("TexturePool");
		ImGui::PopFont();
		ImGui::Text("Alive objects: %d", mTexturePool.AliveObjCount());
		mTexturePool.ForEachAlive([](const TextureHandle& h, TextureEntry& entry)
			{
				TextureDesc desc = entry.Desc;
				String texText = std::format("{} ({}, {})", desc.DebugName, h.Index, h.Generation);
				if (ImGui::TreeNode(texText.c_str()))
				{
					ImGui::Text("Debug Name: %s", desc.DebugName.c_str());
					ImGui::Text("Dimensions: %dx%d", desc.Width, desc.Height);
					ImGui::Text("Format: %s", VulkanUtils::TextureFormatToString(desc.Format).c_str());
					ImGui::Text("Usage: %s", VulkanUtils::TextureUsageToString(desc.Usage));
					ImGui::Text("Size: %.5f MB", entry.Size / (1024.0f * 1024.0f));
					//ImTextureID id = mImGuiContext.AddImage(entry.View);
					//ImGui::Image(id, ImVec2(desc.Width, desc.Height));
					ImGui::TreePop();
				}
			});

		ImGui::End();
	}

	void VulkanRHI::CreateInstance()
	{
		Log<Severity::Info>("Initializing vulkan instance....");

		VK_CALL(volkInitialize());

		Vector<const char*> requiredInstanceExtensions = GetRequiredInstanceExtensions();
		Vector<const char*> requestedInstanceLayers = GetRequiredInstanceLayers();

		VkApplicationInfo app{
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pApplicationName = "SurgePlayer",
			.pEngineName = "SurgeEngine",
			.apiVersion = VK_API_VERSION_1_1 };

		VkInstanceCreateInfo instanceinfo{
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pApplicationInfo = &app,
			.enabledLayerCount = static_cast<Uint>(requestedInstanceLayers.size()),
			.ppEnabledLayerNames = requestedInstanceLayers.data(),
			.enabledExtensionCount = static_cast<Uint>(requiredInstanceExtensions.size()),
			.ppEnabledExtensionNames = requiredInstanceExtensions.data() };

		ENABLE_IF_VK_VALIDATION(mDebugger.Create(instanceinfo));

		// Create the Vulkan instance
		VK_CALL(vkCreateInstance(&instanceinfo, nullptr, &mInstance));
		volkLoadInstance(mInstance);

		ENABLE_IF_VK_VALIDATION(mDebugger.StartDiagnostics(mInstance));
	}

	void VulkanRHI::FillStats()
	{
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(mDevice.GetGPU(), &props);

		mStats.GPUName = props.deviceName;

		Uint major = VK_VERSION_MAJOR(props.apiVersion);
		Uint minor = VK_VERSION_MINOR(props.apiVersion);
		Uint patch = VK_VERSION_PATCH(props.apiVersion);
		mStats.RHIVersion = std::format("Vulkan Version: {}.{}.{}", major, minor, patch);
		
		mStats.VendorName = GetVendorName(props.vendorID);
	}

	void VulkanRHI::CreateSurface(Window* window)
	{
#ifdef SURGE_PLATFORM_WINDOWS
		PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR;

		// Getting the vkCreateWin32SurfaceKHR function pointer and assert if it doesnt exist
		vkCreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)vkGetInstanceProcAddr(mInstance, "vkCreateWin32SurfaceKHR");		
		SG_ASSERT(vkCreateWin32SurfaceKHR, "[Win32] Vulkan instance missing VK_KHR_win32_surface extension");

		VkWin32SurfaceCreateInfoKHR sci;
		memset(&sci, 0, sizeof(sci)); // Clear the info
		sci.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		sci.hinstance = GetModuleHandle(nullptr);
		sci.hwnd = static_cast<HWND>(window->GetNativeWindowHandle());

		vkCreateWin32SurfaceKHR(mInstance, &sci, nullptr, &mSurface);
#elif defined(SURGE_PLATFORM_ANDROID)
		PFN_vkCreateAndroidSurfaceKHR vkCreateAndroidSurfaceKHR = (PFN_vkCreateAndroidSurfaceKHR)vkGetInstanceProcAddr(mInstance, "vkCreateAndroidSurfaceKHR");
		if (!vkCreateAndroidSurfaceKHR)
			SG_ASSERT_INTERNAL("[Android] Vulkan instance missing VK_KHR_android_surface extension");

		VkAndroidSurfaceCreateInfoKHR sci{};
		memset(&sci, 0, sizeof(sci)); // Clear the info
		sci.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
		sci.window = static_cast<ANativeWindow*>(window->GetNativeWindowHandle());

		vkCreateAndroidSurfaceKHR(mInstance, &sci, nullptr, &mSurface);
#else
		SG_ASSERT_INTERNAL("Surge is currently Windows/Android Only! :(");
#endif
	}

	void VulkanRHI::DestroySurface()
	{
		if (mSurface != VK_NULL_HANDLE)
		{
			vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
			mSurface = VK_NULL_HANDLE;
		}

	}

	void VulkanRHI::CreateSwapchainFramebuffers()
	{
		Uint imageCount = mSwapchain.GetImageCount();
		mSwapchainFramebuffers.resize(imageCount);

		// Create framebuffer for each swapchain image view
		for (Uint i = 0; i < imageCount; i++)
		{
			VkImageView attachment = mSwapchain.GetFrame(i).View;

			VkFramebufferCreateInfo fbInfo = {};
			fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			fbInfo.renderPass = mRenderPass;
			fbInfo.attachmentCount = 1;
			fbInfo.pAttachments = &attachment;
			fbInfo.width = mSwapchain.GetWidth();
			fbInfo.height = mSwapchain.GetHeight();
			fbInfo.layers = 1;

			VK_CALL(vkCreateFramebuffer(mDevice.GetDevice(), &fbInfo, nullptr, &mSwapchainFramebuffers[i]));		
		}
	}

	void VulkanRHI::DestroySwapchainFramebuffers()
	{
		for (auto& fb : mSwapchainFramebuffers)
		{
			if (fb != VK_NULL_HANDLE)
				vkDestroyFramebuffer(mDevice.GetDevice(), fb, nullptr);
		}
		mSwapchainFramebuffers.clear();
	}

	void VulkanRHI::CreateSwapchainRenderpass()
	{
		VkDevice device = mDevice.GetDevice();		
		// Proudly stolen from Sascha Willems' Vulkan examples:
		VkAttachmentDescription attachment
		{
			.format = mSwapchain.GetDimensions().Format,               // Backbuffer format
			.samples = VK_SAMPLE_COUNT_1_BIT,                          // Not multisampled
			.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,                      // We blit, so LOAD_OP_LOAD to preserve the blit contents
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,                   // When ending the frame, we want tiles to be written out
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,          // Don't care about stencil since we're not using it
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,        // Don't care about stencil since we're not using it
			.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // The image layout will be undefined when the render pass begins
			.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR             // After the render pass is complete, we will transition to PRESENT_SRC_KHR layout.
		};

		// We have one subpass. This subpass has one color attachment.
		// While executing this subpass, the attachment will be in attachment optimal layout.
		VkAttachmentReference colorRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

		// We will end up with two transitions.
		// The first one happens right before we start subpass #0, where
		// UNDEFINED is transitioned into COLOR_ATTACHMENT_OPTIMAL.
		// The final layout in the render pass attachment states PRESENT_SRC_KHR, so we
		// will get a final transition from COLOR_ATTACHMENT_OPTIMAL to PRESENT_SRC_KHR.
		VkSubpassDescription subpass
		{
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.colorAttachmentCount = 1,
			.pColorAttachments = &colorRef,
		};

		// Create a dependency to external events.
		// We need to wait for the WSI semaphore to signal.
		// Only pipeline stages which depend on COLOR_ATTACHMENT_OUTPUT_BIT will
		// actually wait for the semaphore, so we must also wait for that pipeline stage.
		VkSubpassDependency dependency{
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

		// Since we changed the image layout, we need to make the memory visible to
		// color attachment to modify.
		dependency.srcAccessMask = 0;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		// Finally, create the renderpass.
		VkRenderPassCreateInfo rp_info{
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.attachmentCount = 1,
			.pAttachments = &attachment,
			.subpassCount = 1,
			.pSubpasses = &subpass,
			.dependencyCount = 1,
			.pDependencies = &dependency };

		VK_CALL(vkCreateRenderPass(device, &rp_info, nullptr, &mRenderPass));
	}

	void VulkanRHI::DestroySwapchainRenderpass()
	{
		if (mRenderPass != VK_NULL_HANDLE)
			vkDestroyRenderPass(mDevice.GetDevice(), mRenderPass, nullptr);
	}

	void VulkanRHI::ResizeInternal()
	{
		vkDeviceWaitIdle(mDevice);
		DestroySwapchainFramebuffers();
		mSwapchain.Resize(*this, 0, 0);
		CreateSwapchainFramebuffers();
	}

	Vector<const char*> VulkanRHI::GetRequiredInstanceExtensions()
	{
		Vector<const char*> requiredInstanceExtensions;
		requiredInstanceExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#if defined(SURGE_PLATFORM_ANDROID)
		requiredInstanceExtensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#elif defined(SURGE_PLATFORM_WINDOWS)
		requiredInstanceExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(SURGE_PLATFORM_APPLE)
		requiredInstanceExtensions.push_back(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
#else
#pragma error "Platform not supported"
#endif
		ENABLE_IF_VK_VALIDATION(mDebugger.AddValidationExtensions(requiredInstanceExtensions));

		Uint instanceExtensionCount = 0;
		VK_CALL(vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, nullptr));
		Vector<VkExtensionProperties> availableInstanceExtensions(instanceExtensionCount);
		VK_CALL(vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, availableInstanceExtensions.data()));
		if (!ValidateExtensions(requiredInstanceExtensions, availableInstanceExtensions))
		{
			throw std::runtime_error("Required instance extensions are missing!");
		}

		return requiredInstanceExtensions;
	}

	Vector<const char*> VulkanRHI::GetRequiredInstanceLayers()
	{
		Vector<const char*> instanceLayers;
		ENABLE_IF_VK_VALIDATION(mDebugger.AddValidationLayers(instanceLayers));
		return instanceLayers;
	}

}
