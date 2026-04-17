// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Renderer/UIRenderer.hpp"

namespace Surge
{
    void UIRenderer::Initialize()
    {
        // The ImGui context is currently managed by the legacy VulkanRenderContext.
        // Once the new RHI backend takes ownership this method will call
        // ImGui_ImplVulkan_Init() with the new backend's device and render pass.
    }

    void UIRenderer::Shutdown()
    {
        // ImGui_ImplVulkan_Shutdown();
        // ImGui_ImplGlfw_Shutdown();
        // ImGui::DestroyContext();
    }

    void UIRenderer::BeginFrame()
    {
        // ImGui_ImplVulkan_NewFrame();
        // ImGui_ImplGlfw_NewFrame();
        // ImGui::NewFrame();
    }

    void UIRenderer::EndFrame()
    {
        // ImGui::Render();
        // ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuffer);
    }

    void* UIRenderer::GetTextureID(RHI::TextureHandle /*handle*/)
    {
        // TODO: return ImGui_ImplVulkan_AddTexture() result for the given handle
        return nullptr;
    }

} // namespace Surge
