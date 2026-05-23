// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Memory.hpp"
#include "Surge/Graphics/RenderGraph/RenderGraph.hpp"
#include "Surge/ECS/Components.hpp"
#include "Surge/Graphics/Shader/ShaderManager.hpp"
#include "Surge/Graphics/RHI/RHI.hpp"
#include <imgui.h>

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

        void BeginFrame(const RuntimeCamera& camera, const glm::mat4& transform, Uint submitCount3D = 0);
        void BeginFrame(const EditorCamera& camera, Uint submitCount3D = 0);
        void EndFrame();

        void SubmitQuad(const glm::mat4& transform, const glm::vec4& color, ImageHandle texture = ImageHandle::Invalid())
        {
            FrameBlackboard& bb = mGraph.GetBlackboard();
            bb.QuadList.push_back(QuadSubmitCmd { .Transform = transform, .Color = color, .Texture = texture });
        }
        void SubmitMesh(const glm::mat4& transform, const Ref<Mesh>& mesh)
        {
            FrameBlackboard& bb = mGraph.GetBlackboard();
            bb.MeshList.emplace_back(MeshSubmitCmd { transform, mesh });
        }
        void SubmitLight(const LightComponent& light, const glm::vec3& position, const glm::vec3& rotation)
        {
            FrameBlackboard& bb = mGraph.GetBlackboard();

            Light gpuLight {};
            gpuLight.PositionType = (light.Type == LightType::DIRECTIONAL) ? glm::vec4(rotation, 0.0f) : glm::vec4(position, 1.0f); // Directional light has w = 0, Point light has w = 1
            gpuLight.Color = light.Color;
            gpuLight.Intensity = light.Intensity;
            gpuLight.Radius = light.Radius;
            gpuLight.Falloff = light.Falloff;
            bb.LightList.emplace_back(gpuLight);
        }

        void OnWindowResize(Uint width, Uint height);
        Ref<Material> CreateMaterial(const String& debugName = "Material");

        const FrameBlackboard& GetRenderGraphBlackBoard() { return mGraph.GetBlackboard(); }
        ImageHandle GetWhiteTexture() const { return mGraph.GetBlackboard().WhiteImage; }
        ImageHandle GetFinalImage() const { return mGraph.GetBlackboard().FinalImage; }
        FramebufferHandle GetFinalFramebuffer() const { return mGraph.GetBlackboard().OffscreenFramebuffer; }
        ImTextureID GetFinalImageImGuiID() const
        {
            SG_ASSERT(!RHISettings::BLIT_TO_SWAPCHAIN, "Renderer is blitting to swapchain, cannot get Renderer's final image for ImGui rendering! Set ClientOptions::RenderFinalImageToSwapchain to false");
            return mRHI->GetImGuiImage(mGraph.GetBlackboard().FinalImage);
        }

        ShaderManager& GetShaderManager() { return mShaderManager; }
        SamplerHandle GetDefaultSampler() const { return mGraph.GetBlackboard().DefaultSampler; }

        const Scope<GraphicsRHI>& GetRHI() const { return mRHI; }
        Scope<GraphicsRHI>& GetRHI() { return mRHI; }

        void AddImGuiRenderCallback(std::function<void()> callback) { if (callback) { mImGuiRenderCallbacks.push_back(callback); } }
    private:
        void OnImGuiRender();

    private:
        FrameContext mCurrentFrameCtx;
        Vector<std::function<void()>> mImGuiRenderCallbacks;

        RenderGraph mGraph;

        ShaderManager mShaderManager;
        Scope<GraphicsRHI> mRHI;
    };
} // namespace Surge