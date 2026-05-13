// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Renderer/Renderer3D.hpp"
#include "Surge/Graphics/Camera/EditorCamera.hpp"
#include "Surge/Core/Core.hpp"

namespace Surge
{
    void Renderer3D::Initialize(GraphicsRHI* rhi, RendererData* data)
    {
        SURGE_PROFILE_FUNC("Renderer3D::Initialize()");
        mRHI = rhi;
        mData = data;

		Shader shader;
		shader.Load("Renderer3D.glsl", ShaderType::VERTEX | ShaderType::FRAGMENT);

		PipelineDesc desc = {};
		desc.Shader_ = shader;
		desc.Raster.Topo = Topology::TRIANGLE_LIST;
		desc.Raster.Polygon = PolygonMode::FILL;
		desc.Raster.Cull = CullMode::BACK;
		desc.DebugName = "Renderer3D Pipeline";
		desc.TargetFramebuffer = mData->mOffscreenFramebuffer;
		desc.TargetSwapchain = false;
		desc.Blend.Enable = true;
		mPipeline = mRHI->CreatePipeline(desc);		
    }

	void Renderer3D::BeginFrame(const FrameContext& frameCtx)
	{
		SURGE_PROFILE_FUNC("Renderer3D::BeginFrame(FrameContext)");	
		mCurrentFrameCtx = frameCtx;

		mRHI->BindBindlessSet(mCurrentFrameCtx, mPipeline);
	}

    void Renderer3D::EndFrame()
    {
        SURGE_PROFILE_FUNC("Renderer::EndFrame()");

		mRHI->CmdBindPipeline(mCurrentFrameCtx, mPipeline);

		for (const auto& cmd : mMeshDrawCommands)
		{
			const Mesh& mesh = *cmd.Mesh;
			mRHI->CmdBindVertexBuffer(mCurrentFrameCtx, mesh.GetVertexBuffer());
			mRHI->CmdBindIndexBuffer(mCurrentFrameCtx, mesh.GetIndexBuffer());

			const Submesh* submeshes = mesh.GetSubmeshes().data();
			for (Uint i = 0; i < mesh.GetSubmeshes().size(); i++)
			{
				const Submesh& submesh = submeshes[i];
				glm::mat4 meshData[2] = { cmd.Transform * submesh.Transform, mData->ViewProjection };
				mRHI->CmdPushConstants(mCurrentFrameCtx, mPipeline, ShaderType::VERTEX, 0, sizeof(glm::mat4) * 2, meshData);
				mRHI->CmdDrawIndexed(mCurrentFrameCtx, submesh.IndexCount, 1, submesh.BaseIndex, submesh.BaseVertex, 0);
			}
		}
		mMeshDrawCommands.clear();
    }

	void Renderer3D::SubmitMesh(const glm::mat4& transform, const Ref<Mesh>& mesh)
	{
		mMeshDrawCommands.emplace_back(MeshDrawCmd{ transform, mesh });
	}

	void Renderer3D::OnImGuiRender()
	{
	}

	void Renderer3D::OnWindowResize(Uint width, Uint height)
	{
		Log<Severity::Debug>("WindowResized // Latest dimensions: Width:{0} Height:{1}", width, height);
	}
	
	void Renderer3D::Shutdown()
    {
        SURGE_PROFILE_FUNC("Renderer3D::Shutdown()");
		mRHI->DestroyPipeline(mPipeline);
    }

} // namespace Surge
