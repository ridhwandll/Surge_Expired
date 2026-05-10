// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include <volk.h>
#include <imgui.h>


namespace Surge
{
	class VulkanRHI;
	class VulkanImGuiContext
	{
	public:
		void Init(const VulkanRHI& rhi);
		void Shutdown(const VulkanRHI& rhi);

		// Call once per frame before any ImGui:: widget calls
		void BeginFrame();
		ImTextureID AddImage(VkImageView view);
		// Call once per frame after all ImGui:: widget calls
		// Must be called while swapchain render pass is active
		void EndFrame(VkCommandBuffer cmd);

		bool IsInitialized() const { return mInitialized; }
		ImFont* GetBoldFont() { return mBoldFont; }

	private:
		void SetDarkThemeColors();
	private:
		Vector<ImTextureID> mImageTextureIDs;
		VkDescriptorPool mDescriptorPool = VK_NULL_HANDLE;
		bool mInitialized = false;
		ImFont* mBoldFont = nullptr;
	};

}