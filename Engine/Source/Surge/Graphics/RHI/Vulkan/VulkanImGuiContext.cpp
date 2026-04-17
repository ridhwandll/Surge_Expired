// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/RHI/Vulkan/VulkanImGuiContext.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanImage.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanRHIDevice.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanResources.hpp"
#include "Surge/Core/Core.hpp"
#include <ImGui/Backends/imgui_impl_vulkan.h>
#include <ImGui/Backends/imgui_impl_win32.h>
#include <ImGui/ImGuizmo.h>
#include <IconsFontAwesome.hpp>

namespace Surge
{
    static void ImGuiCheckVkResult(VkResult err) { VK_CALL(err); }

    void VulkanImGuiContext::Initialize(RHI::SwapchainHandle swapchain)
    {
        mSwapchain = swapchain;
        VkDevice logicalDevice = RHI::gVulkanRHIDevice.GetLogicalDevice();

        VkDescriptorPoolSize poolSizes[] =
            {{VK_DESCRIPTOR_TYPE_SAMPLER, 100},
             {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100},
             {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 100},
             {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 100},
             {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 100},
             {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 100},
             {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100},
             {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100},
             {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 100},
             {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 100},
             {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 100}};

        VkDescriptorPoolCreateInfo poolInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = 100 * IM_ARRAYSIZE(poolSizes);
        poolInfo.poolSizeCount = (Uint)IM_ARRAYSIZE(poolSizes);
        poolInfo.pPoolSizes = poolSizes;
        VK_CALL(vkCreateDescriptorPool(logicalDevice, &poolInfo, nullptr, &mImguiPool));

        IMGUI_CHECKVERSION();
        mImGuiContext = ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        constexpr float fontSize = 14.0f;
        io.Fonts->AddFontFromFileTTF("Engine/Assets/Fonts/OpenSans-Bold.ttf", fontSize);
        io.Fonts->AddFontFromFileTTF("Engine/Assets/Fonts/OpenSans-Bold.ttf", 24.0f);
        io.Fonts->AddFontFromFileTTF("Engine/Assets/Fonts/OpenSans-Bold.ttf", fontSize + 3.0f);

        io.FontDefault = io.Fonts->AddFontFromFileTTF("Engine/Assets/Fonts/OpenSans-Regular.ttf", fontSize);

        static const ImWchar iconsRanges[] = {ICON_MIN_FK, ICON_MAX_FK, 0};
        ImFontConfig iconsConfig {};
        iconsConfig.MergeMode = true;
        io.Fonts->AddFontFromFileTTF("Engine/Assets/Fonts/fontawesome-webfont.ttf", fontSize, &iconsConfig, iconsRanges);

        ImGui::StyleColorsDark();

        ImGuiStyle& style = ImGui::GetStyle();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        const RHI::VulkanSwapchain& sc = RHI::GetVulkanRegistry().GetSwapchain(swapchain);

        ImGui_ImplWin32_Init(Core::GetWindow()->GetNativeWindowHandle());
        ImGui_ImplVulkan_InitInfo initInfo {};
        initInfo.Instance = RHI::gVulkanRHIDevice.GetInstance();
        initInfo.PhysicalDevice = RHI::gVulkanRHIDevice.GetPhysicalDevice();
        initInfo.Device = logicalDevice;
        initInfo.QueueFamily = RHI::gVulkanRHIDevice.GetGraphicsFamily();
        initInfo.Queue = RHI::gVulkanRHIDevice.GetGraphicsQueue();
        initInfo.DescriptorPool = mImguiPool;
        initInfo.MinImageCount = 2;
        initInfo.ImageCount = RHI::FRAMES_IN_FLIGHT;
        initInfo.Allocator = VK_NULL_HANDLE;
        initInfo.PipelineCache = VK_NULL_HANDLE;
        initInfo.CheckVkResultFn = ImGuiCheckVkResult;
        ImGui_ImplVulkan_Init(&initInfo, sc.RenderPass);

        RHI::gVulkanRHIDevice.InstantSubmit(RHI::QueueType::Graphics, [&](VkCommandBuffer cmd) {
            ImGui_ImplVulkan_CreateFontsTexture(cmd);
        });

        ImGui_ImplVulkan_DestroyFontUploadObjects();
        SetDarkThemeColors();
    }

    void VulkanImGuiContext::Destroy()
    {
        vkDestroyDescriptorPool(RHI::gVulkanRHIDevice.GetLogicalDevice(), mImguiPool, nullptr);
        mImguiPool = VK_NULL_HANDLE;
        ImGui_ImplWin32_Shutdown();
        ImGui_ImplVulkan_Shutdown();
        ImGui::DestroyContext();
    }

    void VulkanImGuiContext::BeginFrame()
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();
    }

    void VulkanImGuiContext::Render()
    {
        const RHI::VulkanSwapchain& sc = RHI::GetVulkanRegistry().GetSwapchain(mSwapchain);
        uint32_t frameIdx = sc.CurrentFrameIdx;
        VkCommandBuffer cmd = sc.Frames[frameIdx].CmdBuf;

        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
    }

    void VulkanImGuiContext::EndFrame()
    {
        // Empty – presentation happens in the swapchain EndFrame.
    }

    void* VulkanImGuiContext::AddImage(const Ref<Image2D>& image2d) const
    {
        VkDevice device = RHI::gVulkanRHIDevice.GetLogicalDevice();
        const RHI::VulkanSwapchain& sc = RHI::GetVulkanRegistry().GetSwapchain(mSwapchain);
        uint32_t frameIdx = sc.CurrentFrameIdx;

        VkDescriptorSetLayout descriptorSetLayout = ImGui_ImplVulkan_GetDescriptorSetLayout();

        VkDescriptorSetAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &descriptorSetLayout;
        allocInfo.descriptorPool = RHI::gVulkanRHIDevice.GetDescriptorPool(frameIdx);
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        VK_CALL(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));

        Ref<VulkanImage2D> vulkanImage2d = image2d.As<VulkanImage2D>();
        ImGui_ImplVulkan_AddTexture(vulkanImage2d->GetVulkanImageView(), vulkanImage2d->GetVulkanImageLayout(), vulkanImage2d->GetVulkanSampler(), descriptorSet);
        return descriptorSet;
    }

    void VulkanImGuiContext::SetDarkThemeColors()
    {
        constexpr auto colorFromBytes = [](const uint8_t r, const uint8_t g, const uint8_t b) {
            return ImVec4(static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f, static_cast<float>(b) / 255.0f, 1.0f);
        };

        ImVec4 themeColor = colorFromBytes(32, 32, 32);

        auto& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        style.TabRounding = 1.5f;
        style.FrameRounding = 1.5f;
        style.PopupRounding = 3.5f;
        style.ScrollbarRounding = 3.5f;
        style.GrabRounding = 3.5f;
        style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
        style.DisplaySafeAreaPadding = ImVec2(0, 0);

        colors[ImGuiCol_Header] = colorFromBytes(51, 51, 51);
        colors[ImGuiCol_HeaderHovered] = {0.3f, 0.3f, 0.3f, 0.3f};
        colors[ImGuiCol_HeaderActive] = colorFromBytes(22, 22, 22);
        colors[ImGuiCol_CheckMark] = colorFromBytes(10, 200, 10);

        colors[ImGuiCol_Button] = themeColor;
        colors[ImGuiCol_ButtonHovered] = colorFromBytes(66, 66, 66);
        colors[ImGuiCol_ButtonActive] = colorFromBytes(120, 120, 120);
        colors[ImGuiCol_SeparatorHovered] = {0.8f, 0.4f, 0.1f, 1.0f};
        colors[ImGuiCol_SeparatorActive] = {1.0f, 0.5f, 0.1f, 1.0f};

        colors[ImGuiCol_FrameBg] = colorFromBytes(51, 51, 51);
        colors[ImGuiCol_FrameBgHovered] = colorFromBytes(55, 55, 55);
        colors[ImGuiCol_FrameBgActive] = colorFromBytes(110, 110, 110);

        colors[ImGuiCol_Tab] = colorFromBytes(56, 56, 56);
        colors[ImGuiCol_TabHovered] = colorFromBytes(56, 56, 56);
        colors[ImGuiCol_TabActive] = colorFromBytes(90, 90, 90);
        colors[ImGuiCol_TabUnfocused] = colorFromBytes(40, 40, 40);
        colors[ImGuiCol_TabUnfocusedActive] = colorFromBytes(88, 88, 88);

        colors[ImGuiCol_TitleBg] = colorFromBytes(40, 40, 40);
        colors[ImGuiCol_TitleBgActive] = colorFromBytes(40, 40, 40);

        colors[ImGuiCol_WindowBg] = themeColor;
        colors[ImGuiCol_PopupBg] = colorFromBytes(45, 45, 45);
        colors[ImGuiCol_DockingPreview] = colorFromBytes(26, 26, 26);
        colors[ImGuiCol_TitleBg] = {0.12, 0.12, 0.12, 1.0};
        colors[ImGuiCol_TitleBgActive] = {0.14, 0.14, 0.14, 1.0};
    }
} // namespace Surge
