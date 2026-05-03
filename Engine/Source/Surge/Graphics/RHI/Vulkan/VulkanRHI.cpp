// Copyright (c) - SurgeTechnologies - All rights reserved
#include "VulkanRHI.hpp"
#include "VulkanDebugger.hpp"
#include "VulkanBuffer.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Core/Logger/Logger.hpp"


#ifdef SURGE_PLATFORM_ANDROID
#include <android/native_window.h>
#endif

namespace Surge
{
	void VulkanRHI::Initialize(Window* window)
	{
		CreateInstance();
		CreateSurface(window);
		mDevice.Initialize(mInstance, mSurface);
		mFrame.Initialize(*this, 3);
		mSwapchain.Init(*this, window->GetSize().x, window->GetSize().y);

		CreateRenderpass();
		CreateFramebuffers();
	}

	void VulkanRHI::Shutdown()
	{		
		vkDeviceWaitIdle(mDevice.GetDevice()); // When destroying the application, we need to make sure the GPU is no longer accessing any resources

		// Clean up buffers if forgotten to be destroyed manually
		mBufferPool.ForEachAlive([&](const BufferHandle& h, BufferEntry& entry)
		{
			VulkanBuffer::Destroy(*this, entry);
			SG_ASSERT_INTERNAL("You forgot to destroy a buffer manually!");
		});

		DestroyFramebuffers();
		DestroyRenderpass();

		mFrame.Shutdown(*this);
		mSwapchain.Shutdown(*this);
		ENABLE_IF_VK_VALIDATION(mDebugger.EndDiagnostics(mInstance));
		mDevice.Destroy();
		vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
		vkDestroyInstance(mInstance, nullptr);
	}

	void VulkanRHI::BeginFrame(FrameContext& outCtx)
	{
		VkDevice device = mDevice.GetDevice();

		// Get current frame SLOT (round-robin, predictable)
		PerFrame& frame = mFrame.GetCurrentFrame();

		// Wait for this SLOT's fence
		// This slot was used N frames ago, wait until the GPU is done with it
		vkWaitForFences(device, 1, &frame.Fence, VK_TRUE, UINT64_MAX);


		// Ask swapchain which IMAGE is available
		// This is unpredictable, diver may return any index
		// AcquireSemaphore signals when the image is actually ready to write
		Uint imageIndex = 0;
		VkResult result = mSwapchain.AcquireNextImage(*this, frame.AcquireSemaphore, imageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			// Handle resize
			glm::vec2 size = Core::GetWindow()->GetSize();
			ResizeInternal(size.x, size.y);
			result = mSwapchain.AcquireNextImage(*this, frame.AcquireSemaphore, imageIndex);
		}
		SG_ASSERT(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR, "VulkanRHI: AcquireNextImage failed");

		vkResetFences(device, 1, &frame.Fence);
		vkResetCommandPool(device, frame.CmdPool, 0);

		// Begin recording
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(frame.CmdBuffer, &beginInfo);

		// Set clear color values
		VkClearValue clearValue{ .color = {{0.01f, 0.01f, 0.01f, 1.0f}} };

		Uint swapchainWidth = mSwapchain.GetWidth();
		Uint swapchainHeight = mSwapchain.GetHeight();

		// Begin the render pass.
		// TODO: When RenderGraph is implemented, this will be moved to a separate RenderPass class and the info will be pulled from the RenderGraph's data instead of hardcoded here
		// TODO: REMOVE Renderpass from here
		VkRenderPassBeginInfo rp_begin{
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.renderPass = mRenderPass,
			.framebuffer = mSwapchainFramebuffers[imageIndex],
			.renderArea = {.extent = {.width = swapchainWidth, .height = swapchainHeight}},
			.clearValueCount = 1,
			.pClearValues = &clearValue };

		// We will add draw commands in the same command buffer
		vkCmdBeginRenderPass(frame.CmdBuffer, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport vp{
			.width = static_cast<float>(swapchainWidth),
			.height = static_cast<float>(swapchainHeight),
			.minDepth = 0.0f,
			.maxDepth = 1.0f };
		vkCmdSetViewport(frame.CmdBuffer, 0, 1, &vp); // Set viewport dynamically

		VkRect2D scissor{ .extent = {.width = swapchainWidth, .height = swapchainHeight} };
		vkCmdSetScissor(frame.CmdBuffer, 0, 1, &scissor); // Set scissor dynamically

		outCtx.FrameIndex = mFrame.GetCurrentFrameIndex();
		outCtx.SwapchainIndex = imageIndex;
		outCtx.Width = swapchainWidth;
		outCtx.Height = swapchainHeight;
	}

	void VulkanRHI::EndFrame(const FrameContext& ctx)
	{
		PerFrame& frame = mFrame.GetCurrentFrame();
		VkSemaphore releaseSemaphore = mSwapchain.GetFrame(ctx.SwapchainIndex).ReleaseSemaphore;

		vkCmdEndRenderPass(frame.CmdBuffer);
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

	void VulkanRHI::Resize(Uint width, Uint height)
	{
		DestroyFramebuffers();
		mSwapchain.Resize(*this, width, height);
		CreateFramebuffers();
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

	BufferHandle VulkanRHI::CreateBuffer(const BufferDesc& desc)
	{
		Log<Severity::Trace>("\nVulkanRHI::CreateBuffer:\n Name: {0}\n Size {1} bytes\n", desc.DebugName ? desc.DebugName : "Unnamed", desc.Size);
		BufferEntry entry = VulkanBuffer::Create(*this, desc);
		return mBufferPool.Allocate(std::move(entry));
	}

	TextureHandle VulkanRHI::CreateTexture(const TextureDesc& desc, const void* initialData /*= nullptr*/)
	{
		Log<Severity::Trace>("\nVulkanRHI::CreateTexture of size {0}x{1} with format {2} and usage {3}", desc.Width, desc.Height, static_cast<Uint>(desc.Format), static_cast<Uint>(desc.Usage));
		return TextureHandle::Invalid();
	}

	FramebufferHandle VulkanRHI::CreateFramebuffer(const FramebufferDesc& desc, const void* initialData /*= nullptr*/)
	{
		Log<Severity::Trace>("\nVulkanRHI::CreateFramebuffer of size {0}x{1} with {2} color attachments and depth attachment: {3}", desc.Width, desc.Height, desc.ColorAttachmentCount, desc.HasDepth);
		return FramebufferHandle::Invalid();
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
		
		Log<Severity::Info>("VulkanRHI::DestroyBuffer: Size: {0} bytes", entry->Size);
		VulkanBuffer::Destroy(*this, *entry);// kills VkBuffer + VmaAllocation
		mBufferPool.Free(buffer); // returns slot to free list
	}

	void VulkanRHI::DestroyTexture(TextureHandle texture)
	{
		Log<Severity::Info>("Destroying texture with handle index {0} and generation {1}", texture.Index, texture.Generation);
	}

	void VulkanRHI::DestroyFramebuffer(FramebufferHandle framebuffer)
	{
		Log<Severity::Info>("Destroying framebuffer with handle index {0} and generation {1}", framebuffer.Index, framebuffer.Generation);
	}

	void VulkanRHI::CmdDrawIndexed(const FrameContext& ctx, Uint indexCount, Uint instanceCount, Uint firstIndex, int32_t vertexOffset, Uint firstInstance)
	{
		VkCommandBuffer cmd = mFrame.GetFrame(ctx.FrameIndex).CmdBuffer;
		vkCmdDrawIndexed(cmd, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
	}

	void VulkanRHI::CmdDraw(const FrameContext& ctx, Uint vertexCount, Uint instanceCount, Uint firstVertex, Uint firstInstance)
	{
		VkCommandBuffer cmd = mFrame.GetFrame(ctx.FrameIndex).CmdBuffer;
		vkCmdDraw(cmd, vertexCount, instanceCount, firstVertex, firstInstance);
	}

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
#pragma error Platform not supported
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

	void VulkanRHI::CreateRenderpass()
	{
		VkDevice device = mDevice.GetDevice();		

		VkAttachmentDescription attachment{
			.format = mSwapchain.GetDimensions().Format,        // Backbuffer format.
			.samples = VK_SAMPLE_COUNT_1_BIT,                   // Not multisampled.
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,              // When starting the frame, we want tiles to be cleared.
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,            // When ending the frame, we want tiles to be written out.
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,   // Don't care about stencil since we're not using it.
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, // Don't care about stencil since we're not using it.
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,         // The image layout will be undefined when the render pass begins.
			.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR      // After the render pass is complete, we will transition to PRESENT_SRC_KHR layout.
		};

		// We have one subpass. This subpass has one color attachment.
		// While executing this subpass, the attachment will be in attachment optimal layout.
		VkAttachmentReference color_ref = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

		// We will end up with two transitions.
		// The first one happens right before we start subpass #0, where
		// UNDEFINED is transitioned into COLOR_ATTACHMENT_OPTIMAL.
		// The final layout in the render pass attachment states PRESENT_SRC_KHR, so we
		// will get a final transition from COLOR_ATTACHMENT_OPTIMAL to PRESENT_SRC_KHR.
		VkSubpassDescription subpass{
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.colorAttachmentCount = 1,
			.pColorAttachments = &color_ref,
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

	void VulkanRHI::DestroyRenderpass()
	{
		if (mRenderPass != VK_NULL_HANDLE)
			vkDestroyRenderPass(mDevice.GetDevice(), mRenderPass, nullptr);
	}

	void VulkanRHI::CreateFramebuffers()
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

	void VulkanRHI::DestroyFramebuffers()
	{
		for (auto& fb : mSwapchainFramebuffers)
		{
			if (fb != VK_NULL_HANDLE)
				vkDestroyFramebuffer(mDevice.GetDevice(), fb, nullptr);
		}
		mSwapchainFramebuffers.clear();
	}

	void VulkanRHI::ResizeInternal(Uint width, Uint height)
	{
		VkDevice device = mDevice.GetDevice();

		vkDeviceWaitIdle(device);
		DestroyFramebuffers();
		mSwapchain.Resize(*this, width, height);
		CreateFramebuffers();
	}

}
