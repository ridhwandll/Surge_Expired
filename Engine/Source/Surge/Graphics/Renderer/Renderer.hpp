// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Memory.hpp"
#include "Surge/Graphics/RHI/RHI.hpp"
#include "Surge/Graphics/RenderGraph/RenderGraph.hpp"
#include "Surge/Graphics/Shader/ShaderManager.hpp"
#include "Surge/Graphics/Camera/RuntimeCamera.hpp"

namespace Surge
{
    class Scene;
    class EditorCamera;
    class Renderer
    {
    public:
        Renderer() = default;
        ~Renderer() = default;

        void Initialize();
        void Shutdown();

        void BeginFrame(const RuntimeCamera& camera, const glm::mat4& transform);
        void BeginFrame(const EditorCamera& camera);
        void EndFrame();

        void SubmitQuad(const glm::mat4& transform, const glm::vec4& color, ImageHandle texture = ImageHandle::Invalid())
        {
            mGraph.GetBlackboard().QuadList.emplace_back(QuadSubmitCmd { .Transform = transform, .Color = color, .Texture = texture });
        }
        void SubmitLine(const glm::vec3& point0, const glm::vec3& point1, const glm::vec4& color)
        {
            mGraph.GetBlackboard().LineList.emplace_back(LineSubmitCmd { .P0 = point0, .P1 = point1, .Color = color, });
        }
        void SubmitMesh(const glm::mat4& transform, const Ref<Mesh>& mesh, bool dropShadow)
        {
            FrameBlackboard& bb = mGraph.GetBlackboard();
            bb.MeshList.emplace_back(MeshSubmitCmd{ transform, mesh, dropShadow });
            //bb.OutlineList.emplace_back(OutlineSubmitCmd { transform, mesh }); // Uncomment for seeing outline in Runtime
        }
        void SubmitLight(const Light& light);
        void SubmitEnvironment(Environnment&& env);

        void SubmitMeshOutline(const glm::mat4& transform, const Ref<Mesh>& mesh) { mGraph.GetBlackboard().OutlineList.emplace_back(OutlineSubmitCmd{ transform, mesh }); }

        void OnWindowResize(Uint width, Uint height);
        void ForceResize(Uint width, Uint height);
        void ShowInternalImGui(bool show) { mGraph.ShowInternalImGui(show); }
        void SubmitDirLightDebug(const glm::vec3& origin, const glm::vec3& forward, const glm::vec4& color);
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

        const Scope<GraphicsRHI>& GetRHI() const { return mRHI; }
        Scope<GraphicsRHI>& GetRHI() { return mRHI; }

        void AddImGuiRenderCallback(std::function<void()> callback) { mGraph.AddImGuiRenderCallback(std::move(callback)); }

    private:
        FrameContext mCurrentFrameCtx;

        RenderGraph mGraph;

        ShaderManager mShaderManager;
        Scope<GraphicsRHI> mRHI;
    };
} // namespace Surge