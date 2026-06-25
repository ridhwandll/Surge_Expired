// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Core/Profiler.hpp"
#include "Surge/Graphics/RHI/RHI.hpp"
#include "Surge/Graphics/RenderGraph/Passes/Renderer2DPass.hpp"
#include "Surge/Graphics/RenderGraph/Passes/GeometryPass.hpp"
#include "Surge/Graphics/RenderGraph/Passes/PostProcessPass.hpp"
#include "Surge/Graphics/RenderGraph/Passes/SwapchainPass.hpp"
#include "Surge/Graphics/RenderGraph/Passes/OutlinePass.hpp"
#include "Surge/Graphics/RenderGraph/Passes/SkyPass.hpp"
#include "Surge/Graphics/RenderGraph/Passes/ShadowPass.hpp"

#define ENGINE_SHADER_PATH "Engine/Assets/Shaders"

namespace Surge
{
    void Renderer::Initialize()
    {
        SURGE_PROFILE_FUNC("Renderer::Initialize()");

        const ClientOptions& clientOptions = Core::GetClient()->GetClientOptions();
        RHISettings::RENDER_TO_SWAPCHAIN = clientOptions.RenderFinalImageToSwapchian;

        mShaderManager.Initialize(ENGINE_SHADER_PATH);
        mShaderManager.Load("Renderer2D.glsl");
        mShaderManager.Load("Renderer2DLine.glsl");
        mShaderManager.Load("Renderer2DText.glsl");
        mShaderManager.Load("Renderer3D.glsl");
        mShaderManager.Load("PostProcess.glsl");
        mShaderManager.Load("OutlineMask.glsl");
        mShaderManager.Load("PreethamSky.glsl");
        mShaderManager.Load("Shadow.glsl");
        mShaderManager.Load("Present.glsl");

        mRHI = CreateScope<GraphicsRHI>();
        mRHI->Initialize(Core::GetWindow());

        FrameBlackboard& blackBoard = mGraph.GetBlackboard();

        //Sampler
        {
            SamplerDesc samplerDesc = {};
            samplerDesc.DebugName = "DefaultSampler";
            samplerDesc.Min = FilterMode::NEAREST; // We set to NEAREST as Rendergraph Passes uses this internally
            samplerDesc.Mag = FilterMode::NEAREST;
            samplerDesc.Mip = MipmapMode::LINEAR;
            samplerDesc.WrapU = WrapMode::REPEAT;
            samplerDesc.WrapV = WrapMode::REPEAT;
            samplerDesc.Anisotropy = true;
            samplerDesc.MaxAniso = 4;
            blackBoard.DefaultSampler = mRHI->CreateSampler(samplerDesc);
        }
        {
            SamplerDesc samplerDesc = {};
            samplerDesc.DebugName = "TextSampler";
            samplerDesc.Min = FilterMode::LINEAR;
            samplerDesc.Mag = FilterMode::LINEAR;
            samplerDesc.Mip = MipmapMode::LINEAR;
            samplerDesc.WrapU = WrapMode::CLAMP;
            samplerDesc.WrapV = WrapMode::CLAMP;
            blackBoard.TextSampler = mRHI->CreateSampler(samplerDesc);
        }

        Byte whitePixel[] = { 255, 255, 255, 255 };
        ImageDesc texDesc = {};
        texDesc.Width = 1;
        texDesc.Height = 1;
        texDesc.Format = ImageFormat::RGBA8_UNORM;
        texDesc.Usage = ImageUsage::SAMPLED | ImageUsage::TRANSFER_DST;
        texDesc.DebugName = "WhiteTexture";
        texDesc.InitialData = whitePixel;
        texDesc.GenerateImGuiID = true;
        texDesc.DataSize = sizeof(whitePixel);
        texDesc.Sampler = blackBoard.DefaultSampler;
        blackBoard.WhiteImage = mRHI->CreateImage(texDesc);

        BufferDesc frameUBODesc = {};
        frameUBODesc.Usage = BufferUsage::UNIFORM;
        frameUBODesc.HostVisible = true;
        frameUBODesc.DebugName = "FrameUBO";
        frameUBODesc.Size = sizeof(FrameUBO);
        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
            blackBoard.FrameUBOs[i] = mRHI->CreateBuffer(frameUBODesc);

        mGraph.AddPass<ShadowPass>();
        mGraph.AddPass<OutlinePass>();
        mGraph.AddPass<GeometryPass>();
        mGraph.AddPass<SkyPass>();
        mGraph.AddPass<Renderer2DPass>();
        mGraph.AddPass<PostProcessPass>();
        mGraph.AddPass<SwapchainPass>();
        mGraph.Setup(mRHI.get());
        mGraph.Compile();
    }

    void Renderer::BeginFrame(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec2& cameraNearFar)
    {
        SURGE_PROFILE_FUNC("Renderer::BeginFrame(Camera)");
        FrameBlackboard& blackBoard = mGraph.GetBlackboard();
        blackBoard.Env.HasEnvironment = false; //SHOULD we do it here? Seems hacky

        // Camera setup
        blackBoard.ViewMatrix = viewMatrix;
        blackBoard.ProjectionMatrix = projectionMatrix;
        blackBoard.ViewProjection = blackBoard.ProjectionMatrix * blackBoard.ViewMatrix;
        blackBoard.InverseViewProjection = glm::inverse(blackBoard.ViewProjection);
        blackBoard.CameraPosition = glm::inverse(viewMatrix)[3];
        blackBoard.CameraNearFarPlane = cameraNearFar;
        blackBoard.CameraRight = { viewMatrix[0][0], viewMatrix[1][0], viewMatrix[2][0] };
        blackBoard.CameraUp = { viewMatrix[0][1], viewMatrix[1][1], viewMatrix[2][1] };

        FrameUBO frameData = {};
        frameData.View = blackBoard.ViewMatrix;
        frameData.ViewProjection = blackBoard.ViewProjection;
        frameData.InverseViewProjection = blackBoard.InverseViewProjection;
        frameData.CameraPos = blackBoard.CameraPosition;

        mCurrentFrameCtx = mRHI->BeginFrame();
        mRHI->UploadBuffer(blackBoard.FrameUBOs[mCurrentFrameCtx.FrameIndex], &frameData, sizeof(FrameUBO));
    }

    void Renderer::EndFrame()
    {
        SURGE_PROFILE_FUNC("Renderer::EndFrame()");
        mGraph.Execute(mCurrentFrameCtx);

        mRHI->EndFrame(mCurrentFrameCtx); // Stops command buffer recording & presents image to swapchain
        mGraph.ClearLists();
    }

    void Renderer::OnWindowResize(Uint width, Uint height)
    {
        mGraph.OnWindowResize(width, height);
    }

    void Renderer::ForceResize(Uint width, Uint height)
    {
        mGraph.ForceResize(width, height);
    }

    void Renderer::SubmitDirLightDebug(const glm::vec3& origin, const glm::vec3& forward, const glm::vec4& color)
    {
        const float mainLineLength = 3.0f;
        const float parallelLineLength = 2.0f;
        const float radius = 0.75f;

        glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
        if(glm::abs(glm::dot(forward, worldUp)) > 0.999f)
            worldUp = glm::vec3(0.0f, 0.0f, 1.0f);

        glm::vec3 right = glm::normalize(glm::cross(forward, worldUp));
        glm::vec3 up = glm::normalize(glm::cross(right, forward));

        glm::vec3 mainEnd = origin + (forward * mainLineLength);
        SubmitLine(origin, mainEnd, color);

        glm::vec3 offsets[4] = {
            up * radius,          // Top
            -up * radius,         // Bottom
            right * radius,       // Right
            -right * radius       // Left
        };

        for(int i = 0; i < 4; i++)
        {
            glm::vec3 rayStart = origin + offsets[i];
            glm::vec3 rayEnd = rayStart + (forward * parallelLineLength);
            SubmitLine(rayStart, rayEnd, color);
            glm::vec3 nextRayStart = origin + offsets[(i + 1) % 4];
            SubmitLine(rayStart, nextRayStart, color);
            glm::vec3 arrowBase = mainEnd - (forward * 0.5f);
            SubmitLine(mainEnd, arrowBase + offsets[i] * 0.5f, color);
        }
    }

    void Renderer::Shutdown()
    {
        SURGE_PROFILE_FUNC("Renderer::Shutdown()");
        FrameBlackboard& blackBoard = mGraph.GetBlackboard();

        mRHI->WaitIdle();
        mGraph.Shutdown();

        mRHI->DestroyImage(blackBoard.WhiteImage);

        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
            mRHI->DestroyBuffer(blackBoard.FrameUBOs[i]);

        mRHI->DestroySampler(blackBoard.DefaultSampler);
        mRHI->DestroySampler(blackBoard.TextSampler);
        mRHI->Shutdown();
    }

} // namespace Surge
