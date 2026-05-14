// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Memory.hpp"
#include "Lights.hpp"
#include "Surge/Graphics/Renderer/Renderer2D.hpp"
#include "Surge/Graphics/Renderer/Renderer3D.hpp"

namespace Surge
{
    class Scene;
    struct RendererData
    {
        // Camera
        glm::vec3 CameraPosition;
        glm::mat4 ViewMatrix;
        glm::mat4 ProjectionMatrix;
        glm::mat4 ViewProjection;

		TextureHandle mFinalImage;
        SamplerHandle mDefaultSampler;
		FramebufferHandle mOffscreenFramebuffer;
		glm::vec4 mClearColor = { 0.1f, 0.1f, 0.1f, 1.0f };
    };

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

        void SubmitMesh(const glm::mat4& transform, const Ref<Mesh>& mesh);
		void SubmitQuad(const glm::mat4& transform, const glm::vec4& color, TextureHandle texture = TextureHandle::Invalid());
		void OnWindowResize(Uint width, Uint height);

		TextureHandle GetFinalImage() const { return mData->mFinalImage; }
        FramebufferHandle GetFinalFramebuffer() const { return mData->mOffscreenFramebuffer; }
        ImTextureID GetFinalImageImGuiID() const
		{
			SG_ASSERT(!RHISettings::BLIT_TO_SWAPCHAIN, "Renderer is blitting to swapchain, cannot get Renderer's final image for ImGui rendering! Set ClientOptions::RenderFinalImageToSwapchain to false");
            return mRHI->GetImGuiImage(mData->mFinalImage);
		}

		const Renderer2D& GetRenderer2D() const { return mRenderer2D; }
		SamplerHandle GetDefaultSampler() const { return mData->mDefaultSampler; }

		const Scope<GraphicsRHI>& GetRHI() const { return mRHI; }
        Scope<GraphicsRHI>& GetRHI() { return mRHI; }

		void AddImGuiRenderCallback(std::function<void()> callback) { if (callback) { mImGuiRenderCallbacks.push_back(callback); } }
        RendererData* GetData() { return mData.get(); }
    private:
        void OnImGuiRender();

    private:
        FrameContext mCurrentFrameCtx;
		Vector<std::function<void()>> mImGuiRenderCallbacks;

		Renderer2D mRenderer2D;
        Renderer3D mRenderer3D;

        Scope<GraphicsRHI> mRHI;
        Scope<RendererData> mData;
    };
} // namespace Surge