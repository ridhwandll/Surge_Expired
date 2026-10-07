// Copyright (c) - SurgeTechnologies - All rights reserved
#include "AmbientOcclusionPass.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Core/Profiler.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include <imgui.h>

namespace Surge
{
    namespace
    {
        struct LinearDepthPushConstants
        {
            glm::vec4 DepthParams;
            glm::vec2 FullOverAOSize;
        };

        struct GTAOPushConstants
        {
            glm::vec4 ProjInfo;
            glm::vec2 TexelSize;
            float Radius;
            float FalloffRange;
            float MaxDistance;
            float Intensity;
            int SliceCount;
            int StepsPerSlice;
            int Flags;
        };

        struct BlurPushConstants
        {
            glm::vec2 Direction;
            float Sharpness;
            int Radius;
        };

        constexpr int GTAO_FLAG_PERSPECTIVE = BIT(0);
        constexpr int GTAO_FLAG_ACCURATE_NORMALS = BIT(1);
    }

    void AmbientOcclusionPass::Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard)
    {
        SURGE_PROFILE_FUNC("AmbientOcclusionPass::Setup");
        mRHI = rhi;
        mSceneDepth = blackBoard.MainPassDepthImage;
        mPointClampSampler = blackBoard.PointClampSampler;

        mFullSize = Core::GetWindow()->GetSize();
        mAOSize = ComputeAOSize(blackBoard.AOSettings.ResolutionScale);

        ImageDesc imageDesc = {};
        imageDesc.Width = mAOSize.x;
        imageDesc.Height = mAOSize.y;
        imageDesc.Usage = ImageUsage::COLOR_ATTACHMENT | ImageUsage::SAMPLED;
        imageDesc.Sampler = mPointClampSampler;
        imageDesc.GenerateImGuiID = true;

        imageDesc.Format = ImageFormat::R32_SFLOAT;
        imageDesc.DebugName = "AOLinearDepth";
        mLinearDepth = mRHI->CreateImage(imageDesc);

        imageDesc.Format = ImageFormat::R8_UNORM;
        imageDesc.DebugName = "AmbientOcclusion";
        mAO = mRHI->CreateImage(imageDesc);

        imageDesc.DebugName = "AmbientOcclusionBlur";
        mAOBlur = mRHI->CreateImage(imageDesc);

        blackBoard.AOLinearDepth = mLinearDepth;
        blackBoard.AOImage = mAO;

        // Every pass overwrites the full target, so DONT_CARE skips the clear/load (free on tilers)
        auto createFramebuffer = [this](ImageHandle target, const char* debugName) -> FramebufferHandle
        {
            FramebufferAttachment attachment = {};
            attachment.Handle = target;
            attachment.Load = LoadOp::DONT_CARE;
            attachment.Store = StoreOp::STORE;

            FramebufferDesc fbDesc = {};
            fbDesc.ColorAttachments[0] = attachment;
            fbDesc.ColorAttachmentCount = 1;
            fbDesc.HasDepth = false;
            fbDesc.Width = mAOSize.x;
            fbDesc.Height = mAOSize.y;
            fbDesc.DebugName = debugName;
            return mRHI->CreateFramebuffer(fbDesc);
        };
        mLinearDepthFramebuffer = createFramebuffer(mLinearDepth, "AOLinearDepthFramebuffer");
        mAOFramebuffer = createFramebuffer(mAO, "GTAOFramebuffer");
        mBlurHFramebuffer = createFramebuffer(mAOBlur, "AOBlurHFramebuffer");
        mBlurVFramebuffer = createFramebuffer(mAO, "AOBlurVFramebuffer");

        ShaderManager& shaderManager = Core::GetRenderer()->GetShaderManager();
        PipelineDesc pipelineDesc = {};
        pipelineDesc.Raster.Topo = Topology::TRIANGLE_LIST;
        pipelineDesc.Raster.Polygon = PolygonMode::FILL;
        pipelineDesc.Raster.Cull = CullMode::NONE;
        pipelineDesc.Blend.Enable = false;
        pipelineDesc.Depth.TestEnable = false;
        pipelineDesc.Depth.WriteEnable = false;
        pipelineDesc.Stencil.Enable = false;
        pipelineDesc.TargetSwapchain = false;

        pipelineDesc.Shader_ = shaderManager.Get("AOLinearDepth.glsl");
        pipelineDesc.TargetFramebuffer = mLinearDepthFramebuffer;
        pipelineDesc.DebugName = "AOLinearDepth";
        mLinearDepthPipeline = mRHI->CreatePipeline(pipelineDesc);

        pipelineDesc.Shader_ = shaderManager.Get("GTAO.glsl");
        pipelineDesc.TargetFramebuffer = mAOFramebuffer;
        pipelineDesc.DebugName = "GTAO";
        mGTAOPipeline = mRHI->CreatePipeline(pipelineDesc);

        pipelineDesc.Shader_ = shaderManager.Get("AOBlur.glsl");
        pipelineDesc.TargetFramebuffer = mBlurHFramebuffer;
        pipelineDesc.DebugName = "AOBlur";
        mBlurPipeline = mRHI->CreatePipeline(pipelineDesc);

        mLinearDepthSet = mRHI->CreateDescriptorSet(mLinearDepthPipeline, DescriptorSetSlot::ZERO, DescriptorUpdateFrequency::DYNAMIC, "AOLinearDepth [Set0]");
        mGTAOSet = mRHI->CreateDescriptorSet(mGTAOPipeline, DescriptorSetSlot::ZERO, DescriptorUpdateFrequency::DYNAMIC, "GTAO [Set0]");
        mBlurHSet = mRHI->CreateDescriptorSet(mBlurPipeline, DescriptorSetSlot::ZERO, DescriptorUpdateFrequency::DYNAMIC, "AOBlurH [Set0]");
        mBlurVSet = mRHI->CreateDescriptorSet(mBlurPipeline, DescriptorSetSlot::ZERO, DescriptorUpdateFrequency::DYNAMIC, "AOBlurV [Set0]");
        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
            mDescriptorsDirty[i] = true;

        mImageReads.push_back(mSceneDepth);
        mImageWrites.push_back(mLinearDepth);
        mImageWrites.push_back(mAO);
        mImageWrites.push_back(mAOBlur);
    }

    void AmbientOcclusionPass::Execute(const FrameContext& ctx, const FrameBlackboard& blackBoard)
    {
        SURGE_PROFILE_FUNC("AmbientOcclusionPass::Execute");
        const AmbientOcclusionSettings& settings = blackBoard.AOSettings;
        if(!settings.Enable)
            return;

        // Resolution scale changed (ImGui/gameplay code), rebuild targets at frame end, this frame still uses the old ones
        if(ComputeAOSize(settings.ResolutionScale) != mAOSize && !mRecreatePending)
        {
            mRecreatePending = true;
            const float scale = settings.ResolutionScale;
            Core::AddFrameEndCallback([this, scale]() { RecreateTargets(scale); });
        }

        if(mDescriptorsDirty[ctx.FrameIndex])
            UpdateDescriptorSets(ctx.FrameIndex);

        const glm::mat4& proj = blackBoard.ProjectionMatrix;
        if(proj[0][0] == 0.0f || proj[1][1] == 0.0f)
            return;

        const bool isPerspective = proj[2][3] != 0.0f;

        // 1. Linearize + downsample depth
        {
            LinearDepthPushConstants pc = {};
            pc.DepthParams = glm::vec4(proj[2][2], proj[3][2], isPerspective ? 1.0f : 0.0f, 0.0f);
            pc.FullOverAOSize = glm::vec2(mFullSize) / glm::vec2(mAOSize);
            FullscreenPass(ctx, mLinearDepthFramebuffer, mLinearDepthPipeline, mLinearDepthSet, &pc, sizeof(pc));
            mRHI->CmdTransitionImageLayout(ctx, mLinearDepth, ImageUsage::SAMPLED);
        }

        // 2. GTAO
        {
            GTAOPushConstants pc = {};
            // uv -> view space, see ViewPosition() in GTAO.glsl
            const glm::vec2 offset = isPerspective ? glm::vec2(proj[2][0] - 1.0f, proj[2][1] - 1.0f) : glm::vec2(-1.0f - proj[3][0], -1.0f - proj[3][1]);
            pc.ProjInfo = glm::vec4(2.0f / proj[0][0], 2.0f / proj[1][1], offset.x / proj[0][0], offset.y / proj[1][1]);
            pc.TexelSize = 1.0f / glm::vec2(mAOSize);
            pc.Radius = glm::max(settings.Radius, 0.01f);
            pc.FalloffRange = glm::clamp(settings.FalloffRange, 0.05f, 1.0f);
            pc.MaxDistance = settings.MaxDistance;
            pc.Intensity = settings.Intensity;
            pc.SliceCount = glm::max(settings.SliceCount, 1);
            pc.StepsPerSlice = glm::max(settings.StepsPerSlice, 1);
            pc.Flags = (isPerspective ? GTAO_FLAG_PERSPECTIVE : 0) | (settings.AccurateNormals ? GTAO_FLAG_ACCURATE_NORMALS : 0);
            FullscreenPass(ctx, mAOFramebuffer, mGTAOPipeline, mGTAOSet, &pc, sizeof(pc));
        }

        // 3. Separable bilateral blur, ping-pongs back into mAO
        if(settings.BlurRadius > 0)
        {
            BlurPushConstants pc = {};
            pc.Sharpness = settings.BlurSharpness;
            pc.Radius = settings.BlurRadius;

            mRHI->CmdTransitionImageLayout(ctx, mAO, ImageUsage::SAMPLED);
            pc.Direction = glm::vec2(1.0f / static_cast<float>(mAOSize.x), 0.0f);
            FullscreenPass(ctx, mBlurHFramebuffer, mBlurPipeline, mBlurHSet, &pc, sizeof(pc));

            mRHI->CmdTransitionImageLayout(ctx, mAOBlur, ImageUsage::SAMPLED);
            mRHI->CmdTransitionImageLayout(ctx, mAO, ImageUsage::COLOR_ATTACHMENT); // Orders the horizontal pass reads before the vertical pass writes
            pc.Direction = glm::vec2(0.0f, 1.0f / static_cast<float>(mAOSize.y));
            FullscreenPass(ctx, mBlurVFramebuffer, mBlurPipeline, mBlurVSet, &pc, sizeof(pc));
        }
        // mAO is transitioned to SAMPLED by the RenderGraph before PostProcess
    }

    void AmbientOcclusionPass::FullscreenPass(const FrameContext& ctx, FramebufferHandle framebuffer, PipelineHandle pipeline, DescriptorSetHandle set, const void* pushConstants, Uint pushConstantsSize)
    {
        mRHI->CmdBeginRenderPass(ctx, framebuffer);
        mRHI->CmdBindPipeline(ctx, pipeline);
        mRHI->CmdBindDescriptorSet(ctx, pipeline, set, DescriptorSetSlot::ZERO);
        mRHI->CmdPushConstants(ctx, pipeline, ShaderType::VERTEX | ShaderType::FRAGMENT, 0, pushConstantsSize, pushConstants);
        mRHI->CmdDraw(ctx, 3, 1, 0, 0);
        mRHI->CmdEndRenderPass(ctx, framebuffer);
    }

    void AmbientOcclusionPass::UpdateDescriptorSets(Uint frameIndex)
    {
        auto makeWrite = [this](Uint binding, ImageHandle image) -> DescriptorWrite
        {
            DescriptorWrite write = {};
            write.Binding = binding;
            write.Type = DescriptorType::TEXTURE;
            write.Texture = image;
            write.Sampler = mPointClampSampler;
            return write;
        };

        DescriptorWrite linearDepthWrite = makeWrite(0, mSceneDepth);
        mRHI->UpdateDescriptorSet(mLinearDepthSet, &linearDepthWrite, 1, frameIndex);

        DescriptorWrite gtaoWrite = makeWrite(0, mLinearDepth);
        mRHI->UpdateDescriptorSet(mGTAOSet, &gtaoWrite, 1, frameIndex);

        std::array<DescriptorWrite, 2> blurWrites = { makeWrite(0, mAO), makeWrite(1, mLinearDepth) };
        mRHI->UpdateDescriptorSet(mBlurHSet, blurWrites.data(), static_cast<Uint>(blurWrites.size()), frameIndex);

        blurWrites[0] = makeWrite(0, mAOBlur);
        mRHI->UpdateDescriptorSet(mBlurVSet, blurWrites.data(), static_cast<Uint>(blurWrites.size()), frameIndex);

        mDescriptorsDirty[frameIndex] = false;
    }

    glm::uvec2 AmbientOcclusionPass::ComputeAOSize(float resolutionScale) const
    {
        const float scale = glm::clamp(resolutionScale, 0.25f, 1.0f);
        return glm::max(glm::uvec2(glm::vec2(mFullSize) * scale), glm::uvec2(1));
    }

    void AmbientOcclusionPass::RecreateTargets(float resolutionScale)
    {
        // Images are shared between framebuffers (mAO), so resize them once here and rebuild the framebuffers without touching their images
        mRHI->WaitIdle();
        mAOSize = ComputeAOSize(resolutionScale);

        mRHI->ResizeImage(mLinearDepth, mAOSize.x, mAOSize.y);
        mRHI->ResizeImage(mAO, mAOSize.x, mAOSize.y);
        mRHI->ResizeImage(mAOBlur, mAOSize.x, mAOSize.y);

        mRHI->ResizeFramebuffer(mLinearDepthFramebuffer, mAOSize.x, mAOSize.y, false);
        mRHI->ResizeFramebuffer(mAOFramebuffer, mAOSize.x, mAOSize.y, false);
        mRHI->ResizeFramebuffer(mBlurHFramebuffer, mAOSize.x, mAOSize.y, false);
        mRHI->ResizeFramebuffer(mBlurVFramebuffer, mAOSize.x, mAOSize.y, false);

        // New image views (and possibly a new scene depth view from GeometryPass), refresh every frame's copy lazily
        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
            mDescriptorsDirty[i] = true;

        mRecreatePending = false;
    }

    void AmbientOcclusionPass::Resize(Uint width, Uint height, FrameBlackboard& blackBoard)
    {
        if(width == 0 || height == 0)
            return;

        AmbientOcclusionSettings* settings = &blackBoard.AOSettings;
        Core::AddFrameEndCallback([this, width, height, settings]() { mFullSize = { width, height }; RecreateTargets(settings->ResolutionScale); });
    }

    void AmbientOcclusionPass::OnImGuiRender(FrameBlackboard& blackBoard)
    {
        ImFont* boldFont = ImGui::GetIO().Fonts->Fonts[1];
        ImGui::PushFont(boldFont, 25.0f);
        ImGui::TextUnformatted("Ambient Occlusion (GTAO)");
        ImGui::Separator();
        ImGui::PopFont();

        AmbientOcclusionSettings& settings = blackBoard.AOSettings;
        ImGui::Checkbox("Enable AO", &settings.Enable);
        ImGui::Checkbox("Show AO Only", &blackBoard.PostProcessSettings_.ShowAOOnly);

        constexpr const char* qualityNames[] = { "Low (Mobile)", "Medium", "High", "Ultra (Desktop)" };
        int quality = static_cast<int>(settings.Quality);
        if(ImGui::Combo("Quality", &quality, qualityNames, IM_ARRAYSIZE(qualityNames)))
            settings.ApplyQualityPreset(static_cast<AOQuality>(quality));

        ImGui::SliderFloat("Resolution Scale", &settings.ResolutionScale, 0.25f, 1.0f, "%.2f");
        ImGui::Text("AO Resolution: %ux%u", mAOSize.x, mAOSize.y);
        ImGui::SliderInt("Slices", &settings.SliceCount, 1, 8);
        ImGui::SliderInt("Steps Per Slice", &settings.StepsPerSlice, 1, 16);
        ImGui::Checkbox("Accurate Normals", &settings.AccurateNormals);
        ImGui::SliderInt("Blur Radius", &settings.BlurRadius, 0, 6);
        ImGui::SliderFloat("Blur Sharpness", &settings.BlurSharpness, 0.0f, 32.0f, "%.1f");
        ImGui::Separator();
        ImGui::DragFloat("Radius", &settings.Radius, 0.01f, 0.01f, 10.0f, "%.2f");
        ImGui::SliderFloat("Intensity", &settings.Intensity, 0.1f, 4.0f, "%.2f");
        ImGui::SliderFloat("Falloff Range", &settings.FalloffRange, 0.05f, 1.0f, "%.2f");
        ImGui::DragFloat("Max Distance", &settings.MaxDistance, 0.5f, 1.0f, 1000.0f, "%.1f");
        ImGui::Text("Taps per AO pixel: %d", settings.SliceCount * settings.StepsPerSlice * 2 + (settings.AccurateNormals ? 5 : 1));
    }

    void AmbientOcclusionPass::Shutdown(FrameBlackboard& /*blackBoard*/)
    {
        mRHI->DestroyDescriptorSet(mLinearDepthSet);
        mRHI->DestroyDescriptorSet(mGTAOSet);
        mRHI->DestroyDescriptorSet(mBlurHSet);
        mRHI->DestroyDescriptorSet(mBlurVSet);

        mRHI->DestroyPipeline(mLinearDepthPipeline);
        mRHI->DestroyPipeline(mGTAOPipeline);
        mRHI->DestroyPipeline(mBlurPipeline);

        mRHI->DestroyFramebuffer(mLinearDepthFramebuffer);
        mRHI->DestroyFramebuffer(mAOFramebuffer);
        mRHI->DestroyFramebuffer(mBlurHFramebuffer);
        mRHI->DestroyFramebuffer(mBlurVFramebuffer);

        mRHI->DestroyImage(mLinearDepth);
        mRHI->DestroyImage(mAO);
        mRHI->DestroyImage(mAOBlur);
    }
}
