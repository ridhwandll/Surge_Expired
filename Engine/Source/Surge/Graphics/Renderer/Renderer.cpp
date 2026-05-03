// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Graphics/Camera/EditorCamera.hpp"
#include "Surge/Core/Core.hpp"
#include <exception>
#include "Surge/Utility/Filesystem.hpp"
#include <algorithm>

#ifdef SURGE_PLATFORM_WINDOWS
#include <shaderc/shaderc.hpp>
#elif defined(SURGE_PLATFORM_ANDROID)
#include <android_native_app_glue.h>
#include <android/asset_manager.h>
#include <android/log.h>
#endif

namespace Surge
{
	static const std::vector<Renderer::Vertex> sVertices = {
	{{ 0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}}, // 0 — top right,    red
	{{ 0.5f,  0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}}, // 1 — bottom right, green
	{{-0.5f,  0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}, // 2 — bottom left,  blue
	{{-0.5f, -0.5f, 0.5f}, {1.0f, 1.0f, 0.0f}}, // 3 — top left,     yellow
	};

	static const std::vector<uint32_t> sIndices = {
		0, 1, 2, // triangle 1
		0, 2, 3  // triangle 2
	};

    void Renderer::Initialize()
    {
        SURGE_PROFILE_FUNC("Renderer::Initialize()");
        mData = CreateScope<RendererData>();
		mRHI = CreateScope<GraphicsRHI>();
        mRHI->Initialize(Core::GetWindow());
        
		PipelineDesc desc = {};
		desc.VertShaderName = "Triangle.vert";
		desc.FragShaderName = "Triangle.frag";

		// Must match struct Renderer::Vertex { glm::vec3 position; glm::vec3 color; }
		desc.Bindings[0] = { .Binding = 0, .Stride = sizeof(Vertex) };
		desc.BindingCount = 1;

		// Must match struct Renderer::Vertex { glm::vec3 position; glm::vec3 color; }
		desc.Attributes[0] = { .Location = 0, .Binding = 0, .Format = VertexFormat::FLOAT3, .Offset = offsetof(Vertex, position) };
		desc.Attributes[1] = { .Location = 1, .Binding = 0, .Format = VertexFormat::FLOAT3, .Offset = offsetof(Vertex, color) };
		desc.AttributeCount = 2;

		desc.Raster.Cull = CullMode::NONE;
		desc.Raster.Front = FrontFace::COUNTER_CLOCKWISE;
		desc.DebugName = "TrianglePipeline";
		mPipelineHandle = mRHI->CreatePipeline(desc);

		BufferDesc vbDesc = {};
		vbDesc.DebugName = "TriangleVB";
		vbDesc.HostVisible = true;
		vbDesc.Size = sizeof(sVertices[0]) * sVertices.size();
		vbDesc.Usage = BufferUsage::VERTEX;
		vbDesc.InitialData = sVertices.data();
		mVertexBuffer = mRHI->CreateBuffer(vbDesc);

		BufferDesc ibDesc = {};
		ibDesc.Size = sizeof(Uint) * sIndices.size();
		ibDesc.Usage = BufferUsage::INDEX;
		ibDesc.HostVisible = true;
		ibDesc.InitialData = sIndices.data();
		ibDesc.DebugName = "TriangleIB";
		mIndexBuffer = mRHI->CreateBuffer(ibDesc);
    }

    void Renderer::BeginFrame(const RuntimeCamera& camera, const glm::mat4& transform)
    {
        SURGE_PROFILE_FUNC("Renderer::BeginFrame(Camera)");
        mData->ViewMatrix = glm::inverse(transform);
        mData->ProjectionMatrix = camera.GetProjectionMatrix();
        mData->ViewProjection = mData->ProjectionMatrix * mData->ViewMatrix;
        mData->CameraPosition = transform[3];
    }

    void Renderer::BeginFrame(const EditorCamera& camera)
    {
        SURGE_PROFILE_FUNC("Renderer::BeginFrame(EditorCamera)");
        mData->ViewMatrix = camera.GetViewMatrix();
        mData->ProjectionMatrix = camera.GetProjectionMatrix();
        mData->ViewProjection = mData->ProjectionMatrix * mData->ViewMatrix;
        mData->CameraPosition = camera.GetPosition();
    }

    void Renderer::EndFrame()
    {
        SURGE_PROFILE_FUNC("Renderer::EndFrame()");
        
        FrameContext ctx = mRHI->BeginFrame();
		mRHI->CmdBindPipeline(ctx, mPipelineHandle);
		mRHI->CmdBindVertexBuffer(ctx, mVertexBuffer, 0);
		mRHI->CmdBindIndexBuffer(ctx, mIndexBuffer, 0);
		mRHI->CmdDrawIndexed(ctx, sIndices.size(), 1, 0, 0, 0);
		mRHI->EndFrame(ctx);
    }

    void Renderer::SetRenderArea(Uint width, Uint height) {}
    void Renderer::Shutdown()
    {
        SURGE_PROFILE_FUNC("Renderer::Shutdown()");
        mRHI->InitiateShutdown();
		mRHI->DestroyPipeline(mPipelineHandle);
        mRHI->DestroyBuffer(mVertexBuffer);
        mRHI->DestroyBuffer(mIndexBuffer);
        mRHI->Shutdown();
    }

} // namespace Surge
