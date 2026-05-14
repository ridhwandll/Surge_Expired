// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Core/Core.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanImGui.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanRHI.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanDebugger.hpp"

#if defined(SURGE_PLATFORM_ANDROID)
#include "Surge/Platform/Android/AndroidApp.hpp"
#include <Backends/imgui_impl_android.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <android/asset_manager.h>
#elif defined(SURGE_PLATFORM_WINDOWS)
#include <Backends/imgui_impl_win32.h>
#endif

#include <Backends/imgui_impl_vulkan.h>

namespace Surge
{
#ifdef SURGE_PLATFORM_ANDROID
	ImFont* LoadImGuiFont(const char* filename, int fontSize)
	{
		AAssetManager* assetManager = Android::GAndroidApp->activity->assetManager;
		AAsset* asset = AAssetManager_open(assetManager, filename, AASSET_MODE_BUFFER);
		if (asset)
		{
			off_t size = AAsset_getLength(asset);
			void* data = malloc(size);
			AAsset_read(asset, data, size);
			AAsset_close(asset);

			auto& io = ImGui::GetIO();
			ImFont* font = io.Fonts->AddFontFromMemoryTTF(data, (int)size, fontSize);
            SG_ASSERT(font, "Font file not found!")
            return font;
		}
        return nullptr;
	}
#endif

	static void ImGuiCheckVkResult(VkResult err)
	{
		VK_CALL(err);
	}

	void VulkanImGuiContext::Init(const VulkanRHI& rhi)
	{
		const Uint& swapchainImageCount = rhi.GetSwapchain().GetImageCount();
		VkRenderPass swapchainRenderPass = rhi.GetRenderPass();
		VkInstance instance = rhi.GetInstance();
		VkDevice device = rhi.GetDevice();
		VkPhysicalDevice gpu = rhi.GetGPU();

		void* window = Core::GetWindow()->GetNativeWindowHandle();

		SG_ASSERT(!mInitialized, "ImGuiLayer::Init called twice");
		SG_ASSERT(swapchainRenderPass != VK_NULL_HANDLE, "ImGuiLayer: swapchainRenderPass is null, must be created before ImGui init");

		VkDescriptorPoolSize poolSizes[] =
		{ {VK_DESCRIPTOR_TYPE_SAMPLER, 100},
		 {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100},
		 {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 100},
		 {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 100},
		 {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 100},
		 {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 100},
		 {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100},
		 {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100},
		 {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 100},
		 {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 100},
		 {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 100} };

		VkDescriptorPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.pNext = nullptr;
		poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		poolInfo.maxSets = 100 * IM_ARRAYSIZE(poolSizes);
		poolInfo.poolSizeCount = (Uint)IM_ARRAYSIZE(poolSizes);
		poolInfo.pPoolSizes = poolSizes;
		VK_CALL(vkCreateDescriptorPool(device, &poolInfo, nullptr, &mDescriptorPool));

		vkDeviceWaitIdle(device);

		// ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

		// Style
		ImGui::StyleColorsDark();
		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowRounding = 4.0f;
		style.FrameRounding = 3.0f;
		style.GrabRounding = 3.0f;
		style.WindowBorderSize = 0.0f;

		// Platform backend
#if defined(SURGE_PLATFORM_ANDROID)
		SG_ASSERT(window, "ImGuiLayer: ANativeWindow* is null");
		io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		ImGui_ImplAndroid_Init(static_cast<ANativeWindow*>(window));

		// Mobile larger touch targets
		io.FontGlobalScale = 2.0f;
		ImGui::GetStyle().ScaleAllSizes(2.0f);
		style.TouchExtraPadding = ImVec2(4.0f, 4.0f);

#elif defined(SURGE_PLATFORM_WINDOWS)
		SG_ASSERT(window, "ImGuiLayer: HWND is null");
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

		ImGui_ImplWin32_Init(static_cast<HWND>(window));
		ImGui::GetPlatformIO().Platform_CreateVkSurface = // Stolen from ImGui Vulkan example's main.cpp(we respect their naming convention):
			[](ImGuiViewport* viewport, ImU64 vk_instance, const void* vk_allocator, ImU64* out_vk_surface)
			{
				PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR;
				// Getting the vkCreateWin32SurfaceKHR function pointer and assert if it doesnt exist
				vkCreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)vkGetInstanceProcAddr((VkInstance)vk_instance, "vkCreateWin32SurfaceKHR");
				SG_ASSERT(vkCreateWin32SurfaceKHR, "[Win32] Vulkan instance missing VK_KHR_win32_surface extension");

				VkWin32SurfaceCreateInfoKHR createInfo = {};
				createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
				createInfo.hwnd = (HWND)viewport->PlatformHandleRaw;
				createInfo.hinstance = ::GetModuleHandle(nullptr);
				return (int)vkCreateWin32SurfaceKHR((VkInstance)vk_instance, &createInfo, (VkAllocationCallbacks*)vk_allocator, (VkSurfaceKHR*)out_vk_surface);
			};
#endif

		// Vulkan backend
		ImGui_ImplVulkan_InitInfo vkInfo = {};
		vkInfo.ApiVersion = VK_API_VERSION_1_1;
		vkInfo.Instance = instance;
		vkInfo.PhysicalDevice = gpu;
		vkInfo.Device = device;
		vkInfo.QueueFamily = rhi.GetQueueIndex();
		vkInfo.Queue = rhi.GetQueue();
		vkInfo.DescriptorPool = mDescriptorPool;
		vkInfo.MinImageCount = 2;
		vkInfo.ImageCount = swapchainImageCount;
		vkInfo.UseDynamicRendering = false;
		vkInfo.PipelineInfoMain.RenderPass = swapchainRenderPass;
		vkInfo.PipelineInfoMain.Subpass = 0;
		vkInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
#ifdef SURGE_DEBUG
		vkInfo.CheckVkResultFn = ImGuiCheckVkResult;
#endif
		ImGui_ImplVulkan_Init(&vkInfo);

		// Load Fonts
		style.FontSizeBase = 17.0f;
		
		// ----------------------- ImGui Font Management ----------------------- 
		// Normal font -> index = 0
		// Bold font   -> index = 1
		// Italic font -> index = 2
		// Use ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[index]) to use!
		// ----------------------- ImGui Font Management ----------------------- 

#ifdef SURGE_PLATFORM_WINDOWS
		io.Fonts->AddFontFromFileTTF("Engine/Assets/Fonts/FiraSans-Regular.ttf", style.FontSizeBase);		
		io.Fonts->AddFontFromFileTTF("Engine/Assets/Fonts/FiraSans-SemiBold.ttf", style.FontSizeBase);
		io.Fonts->AddFontFromFileTTF("Engine/Assets/Fonts/FiraSans-Italic.ttf", style.FontSizeBase);
#elif defined(SURGE_PLATFORM_ANDROID)
		LoadImGuiFont("Engine/Assets/Fonts/FiraSans-Regular.ttf", style.FontSizeBase);
		LoadImGuiFont("Engine/Assets/Fonts/FiraSans-SemiBold.ttf", style.FontSizeBase);
		LoadImGuiFont("Engine/Assets/Fonts/FiraSans-Italic.ttf", style.FontSizeBase);
#else
		io.Fonts->AddFontDefault();
#endif

		SetDarkThemeColors();
		mInitialized = true;
		Log<Severity::Debug>("ImGuiLayer initialized ({} swapchain images)", swapchainImageCount);
	}

	void VulkanImGuiContext::Shutdown(const VulkanRHI& rhi)
	{
		if (!mInitialized)
			return;

		VkDevice device = rhi.GetDevice();

		vkDeviceWaitIdle(device);

		ImGui_ImplVulkan_Shutdown();

#if defined(SURGE_PLATFORM_ANDROID)
		ImGui_ImplAndroid_Shutdown();
#elif defined(SURGE_PLATFORM_WINDOWS)
		ImGui_ImplWin32_Shutdown();
#endif

		ImGui::DestroyContext();

		if (mDescriptorPool != VK_NULL_HANDLE)
		{
			vkDestroyDescriptorPool(device, mDescriptorPool, nullptr);
			mDescriptorPool = VK_NULL_HANDLE;
		}

		mInitialized = false;
		Log<Severity::Debug>("ImGuiLayer shutdown");
	}

	void VulkanImGuiContext::BeginFrame()
	{
		SG_ASSERT(mInitialized, "ImGuiLayer: BeginFrame called before Init");
		ImGui_ImplVulkan_NewFrame();

#if defined(SURGE_PLATFORM_ANDROID)
		ImGui_ImplAndroid_NewFrame();
#elif defined(SURGE_PLATFORM_WINDOWS)
		ImGui_ImplWin32_NewFrame();
#endif
		ImGui::NewFrame();
	}

	ImTextureID VulkanImGuiContext::AddImage(VkImageView view)
	{
		VkDescriptorSet id = ImGui_ImplVulkan_AddTexture(view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		return (ImTextureID)id;
	}

	void VulkanImGuiContext::DestroyImage(ImTextureID id)
	{
		ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)id);
	}

	void VulkanImGuiContext::EndFrame(VkCommandBuffer cmd)
	{
		SG_ASSERT(mInitialized, "ImGuiLayer: EndFrame called before Init");
		SG_ASSERT(cmd != VK_NULL_HANDLE, "ImGuiLayer: EndFrame requires an active VkCommandBuffer must be called inside an active render pass");

		ImGui::Render();

		ImDrawData* drawData = ImGui::GetDrawData();
		ImGui_ImplVulkan_RenderDrawData(drawData, cmd);

#ifdef SURGE_PLATFORM_WINDOWS
		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}
#endif	
	}

	void VulkanImGuiContext::SetDarkThemeColors()
	{
		constexpr auto colorFromBytes = [](const uint8_t r, const uint8_t g, const uint8_t b) { return ImVec4(static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f, static_cast<float>(b) / 255.0f, 1.0f); };
		ImVec4 themeColor = colorFromBytes(32, 32, 32);

		auto& style = ImGui::GetStyle();

		ImVec4* colors = style.Colors;

		style.TabRounding = 1.5f;
		style.FrameRounding = 1.5f;
		//style.FrameBorderSize = 1.0f;
		style.PopupRounding = 3.5f;
		style.ScrollbarRounding = 3.5f;
		style.GrabRounding = 3.5f;
		style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
		style.DisplaySafeAreaPadding = ImVec2(0, 0);

		// Headers
		colors[ImGuiCol_Header] = colorFromBytes(51, 51, 51);
		colors[ImGuiCol_HeaderHovered] = { 0.3f, 0.3f, 0.3f, 0.3f };
		colors[ImGuiCol_HeaderActive] = colorFromBytes(22, 22, 22);
		colors[ImGuiCol_CheckMark] = colorFromBytes(10, 200, 10); // Green

		colors[ImGuiCol_Button] = themeColor;
		colors[ImGuiCol_ButtonHovered] = colorFromBytes(66, 66, 66);
		colors[ImGuiCol_ButtonActive] = colorFromBytes(120, 120, 120);
		colors[ImGuiCol_SeparatorHovered] = { 0.8f, 0.4f, 0.1f, 1.0f };
		colors[ImGuiCol_SeparatorActive] = { 1.0f, 0.5f, 0.1f, 1.0f };

		// Frame
		colors[ImGuiCol_FrameBg] = colorFromBytes(51, 51, 51);
		colors[ImGuiCol_FrameBgHovered] = colorFromBytes(55, 55, 55);
		colors[ImGuiCol_FrameBgActive] = colorFromBytes(110, 110, 110);

		// Tabs
		colors[ImGuiCol_Tab] = colorFromBytes(56, 56, 56);
		colors[ImGuiCol_TabHovered] = colorFromBytes(56, 56, 56);
		colors[ImGuiCol_TabActive] = colorFromBytes(90, 90, 90);
		colors[ImGuiCol_TabUnfocused] = colorFromBytes(40, 40, 40);
		colors[ImGuiCol_TabUnfocusedActive] = colorFromBytes(88, 88, 88);

		// Title
		colors[ImGuiCol_TitleBg] = colorFromBytes(40, 40, 40);
		colors[ImGuiCol_TitleBgActive] = colorFromBytes(40, 40, 40);

		// Others
		colors[ImGuiCol_WindowBg] = themeColor;
		colors[ImGuiCol_PopupBg] = colorFromBytes(45, 45, 45);
		colors[ImGuiCol_DockingPreview] = colorFromBytes(26, 26, 26);
		colors[ImGuiCol_TitleBg] = { 0.12, 0.12, 0.12, 1.0 };
		colors[ImGuiCol_TitleBgActive] = { 0.14, 0.14, 0.14, 1.0 };
		//colors[ImGuiCol_Separator] = colorFromBytes(10, 200, 10);
		//colors[ImGuiCol_Border] = colorFromBytes(10, 200, 10);
	}
}
