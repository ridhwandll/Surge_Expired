// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Memory.hpp"
#include "Surge/Graphics/RHI/RHI.hpp"
#include "Surge/Graphics/RenderGraph/RenderGraph.hpp"
#include "Surge/Graphics/Shader/ShaderManager.hpp"
#include "Surge/Graphics/Renderer/Text.hpp"
#include "Surge/Graphics/UISystem/UIManager.hpp"

namespace Surge
{
    class Renderer
    {
    public:
        void Initialize();
        void Shutdown();
        void OnEvent(Event& e);

        void BeginFrame(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec2& cameraNearFar);
        void EndFrame();

        FORCEINLINE void SubmitQuad(const glm::mat4& transform, const glm::vec4& color, bool billboard = false, ImageHandle texture = ImageHandle::Invalid())
        {
            mGraph.GetBlackboard().QuadList.emplace_back(QuadSubmitCmd { .Transform = transform, .Color = color, .Texture = texture, .Billboard = billboard });
        }
        FORCEINLINE void SubmitLine(const glm::vec3& point0, const glm::vec3& point1, const glm::vec4& color)
        {
            mGraph.GetBlackboard().LineList.emplace_back(LineSubmitCmd { .P0 = point0, .P1 = point1, .Color = color, });
        }
        FORCEINLINE void SubmitText(const glm::mat4& transform, const String& txt, const glm::vec4& color, float maxWidth, float letterSpacing,
                        float lineSpacing, TextAlignment alignment, TextVerticalAlignment vAlignment, bool italic, bool underline,
                        bool enableShadow, const glm::vec2& shadowOffset, const glm::vec4& shadowColor, const Font* font, bool billboard = false)
        {
            mGraph.GetBlackboard().TextList.emplace_back(TextSubmitCmd { .Transform = transform, .Text = txt, .Color = color, .MaxWidth = maxWidth, .LetterSpacing = letterSpacing,
                                                         .LineSpacing = lineSpacing, .Alignment = alignment, .VerticalAlignment = vAlignment, .Italic = italic, .Underline = underline, .Billboard = billboard,
                                                          .EnableShadow = enableShadow, .ShadowOffset = shadowOffset, .ShadowColor = shadowColor, .FontAsset = font });
        }
        FORCEINLINE void SubmitMesh(const glm::mat4& transform, const Mesh* mesh, bool dropShadow) { mGraph.GetBlackboard().MeshList.emplace_back(MeshSubmitCmd{ transform, mesh, dropShadow }); }
        FORCEINLINE void SubmitMeshOutline(const glm::mat4& transform, const Mesh* mesh) { mGraph.GetBlackboard().OutlineList.emplace_back(OutlineSubmitCmd{ transform, mesh }); }
        FORCEINLINE void SubmitLight(const Light& light)
        {
            FrameBlackboard& bb = mGraph.GetBlackboard();
            if(light.PositionType.w == (float)LightType::DIRECTIONAL)
            {
                bb.HasDirectionalLight = true;
                bb.DirectionalLightDir = glm::vec3(light.PositionType);
            }
            bb.LightList.emplace_back(light);
        }
        FORCEINLINE void SubmitEnvironment(Environnment&& env)
        {
            env.HasEnvironment = true;
            mGraph.GetBlackboard().Env = std::move(env);
        }

        void OnWindowResize(Uint width, Uint height);
        void ForceResize(Uint width, Uint height);
        void ShowInternalImGui(bool show) { mGraph.ShowInternalImGui(show); }
        void SubmitDirLightDebug(const glm::vec3& origin, const glm::vec3& forward, const glm::vec4& color);
        void ShowUI(bool show) { mGraph.GetBlackboard().ShowUI = show; }

        const UI::Manager& GetUIManager() const { return mUIManager; }
        UI::Manager& GetUIManager() { return mUIManager; }

        const FrameBlackboard& GetRenderGraphBlackBoard() const { return mGraph.GetBlackboard(); }
        FrameBlackboard& GetRenderGraphBlackBoard() { return mGraph.GetBlackboard(); }

        ImageHandle GetWhiteTexture() const { return mGraph.GetBlackboard().WhiteImage; }
        ImageHandle GetFinalImage() const { return mGraph.GetBlackboard().FinalImage; }
        FramebufferHandle GetFinalFramebuffer() const { return mGraph.GetBlackboard().MainPassFramebuffer; }
        uint64_t GetFinalImageImGuiID() const
        {
            SG_ASSERT(!RHISettings::RENDER_TO_SWAPCHAIN, "Renderer is rendering to swapchain, cannot get Renderer's final image for ImGui rendering! Set ClientOptions::RenderFinalImageToSwapchain to false");
            return mRHI->GetImGuiImage(mGraph.GetBlackboard().FinalImage);
        }

        void SetOutlineColor(glm::vec3 outlineColor) { mGraph.GetBlackboard().OutlineColor = outlineColor; }
        void SetOutlineThickness(float outlineThickness) { mGraph.GetBlackboard().OutlineThickness = outlineThickness; }

        ShaderManager& GetShaderManager() { return mShaderManager; }
        SamplerHandle GetDefaultSampler() const { return mGraph.GetBlackboard().DefaultSampler; }
        SamplerHandle GetTextSampler() const { return mGraph.GetBlackboard().TextSampler; }

        const Scope<GraphicsRHI>& GetRHI() const { return mRHI; }
        Scope<GraphicsRHI>& GetRHI() { return mRHI; }

        void AddImGuiRenderCallback(std::function<void()>&& callback) { mGraph.AddImGuiRenderCallback(std::move(callback)); }

    private:
        FrameContext mCurrentFrameCtx;

        glm::uvec2 mScreenSize;
        RenderGraph mGraph;

        UI::Manager mUIManager;
        ShaderManager mShaderManager;

        Scope<GraphicsRHI> mRHI;
    };
} // namespace Surge