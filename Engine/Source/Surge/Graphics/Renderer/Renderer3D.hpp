// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Memory.hpp"
#include "Surge/Graphics/RHI/RHI.hpp"
#include "Surge/Graphics/Camera/RuntimeCamera.hpp"
#include "Surge/Graphics/Mesh/Mesh.hpp"

namespace Surge
{
    class Scene;
    class EditorCamera;
	struct RendererData;
    class Renderer3D
    {
    public:
        Renderer3D() = default;
        ~Renderer3D() = default;

		TextureHandle GetFinalImage() const { return TextureHandle::Invalid(); /*TODO*/ }

    private:
        void Initialize(GraphicsRHI* rhi, RendererData* data);
        void Shutdown();

		// Called by Renderer, not meant to be called directly
        void BeginFrame(const FrameContext& frameCtx);
        void EndFrame();
        void SubmitMesh(const glm::mat4& transform, const Ref<Mesh>& mesh);
		void OnWindowResize(Uint width, Uint height);

        void OnImGuiRender();
    private:
        struct MeshDrawCmd
        {
            glm::mat4 Transform;
            Ref<Mesh> Mesh;
		};

    private:
        FrameContext mCurrentFrameCtx;
        GraphicsRHI* mRHI;
        RendererData* mData;

		PipelineHandle mPipeline;

        Vector<MeshDrawCmd> mMeshDrawCommands;
		friend class Renderer;
    };
} // namespace Surge