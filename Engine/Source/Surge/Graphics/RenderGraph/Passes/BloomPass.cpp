// Copyright (c) - SurgeTechnologies - All rights reserved
#include "BloomPass.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Core/Profiler.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include <imgui.h>

namespace Surge
{
    namespace
    {
        // Must match the push_constant block in Engine/Assets/Shaders/Include/BloomCommon.glsli
        struct BloomPushConstants
        {
            glm::vec4 Threshold;
            glm::vec2 SourceTexelSize;
            glm::vec2 OutputTexelSize;
            float Scatter;
            float ClampValue;
            int Mode;
            int Flags;
        };
        static_assert(sizeof(BloomPushConstants) == 48, "BloomPushConstants must match BloomCommon.glsli");

        constexpr int BLOOM_MODE_PREFILTER = 0;
        constexpr int BLOOM_MODE_DOWNSAMPLE = 1;
        constexpr int BLOOM_MODE_UPSAMPLE = 2;

        constexpr int BLOOM_FLAG_HQ_FILTER = BIT(0);
        constexpr int BLOOM_FLAG_ANTI_FLICKER = BIT(1);

        constexpr Uint MIN_MIP_COUNT = 2;
        constexpr Uint MAX_MIP_COUNT = 8;
        constexpr Uint COMPUTE_GROUP_SIZE = 8; // local_size in BloomCompute.glsl

        glm::uvec2 MipSize(const glm::uvec2& fullSize, Uint mip)
        {
            return glm::max(glm::uvec2(fullSize.x >> (mip + 1), fullSize.y >> (mip + 1)), glm::uvec2(1));
        }
    }

    void BloomPass::Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard)
    {
        SURGE_PROFILE_FUNC("BloomPass::Setup");
        mRHI = rhi;
        mSceneColor = blackBoard.MainPassColorImage;
        mLinearClampSampler = blackBoard.LinearClampSampler;
        mFullSize = Core::GetWindow()->GetSize();

        if(mRHI->SupportsCompute())
        {
            ComputePipelineDesc computeDesc = {};
            computeDesc.Shader_ = Core::GetRenderer()->GetShaderManager().Get("BloomCompute.glsl");
            computeDesc.DebugName = "BloomCompute";
            mComputePipeline = mRHI->CreateComputePipeline(computeDesc);
        }

        BuildChain(ComputeConfig(blackBoard.Bloom));
        blackBoard.BloomImage = mBloomOutput;

        mImageReads.push_back(mSceneColor);
    }

    BloomPass::ChainConfig BloomPass::ComputeConfig(const BloomSettings& settings) const
    {
        ChainConfig config;
        config.UseCompute = settings.UseCompute && !mComputePipeline.IsNull();
        config.FullSize = mFullSize;

        // Stop before the smallest mip collapses below 2px, blurring a 1px image only wastes a pass
        const Uint requested = static_cast<Uint>(glm::clamp(settings.MipCount, static_cast<int>(MIN_MIP_COUNT), static_cast<int>(MAX_MIP_COUNT)));
        Uint count = 0;
        while(count < requested)
        {
            const glm::uvec2 size = { mFullSize.x >> (count + 1), mFullSize.y >> (count + 1) };
            if(size.x < 2 || size.y < 2)
                break;
            count++;
        }
        config.MipCount = glm::max(count, MIN_MIP_COUNT);
        return config;
    }

    void BloomPass::BuildChain(const ChainConfig& config)
    {
        mConfig = config;
        const Uint mipCount = config.MipCount;

        // R11G11B10 halves the bandwidth of RGBA16F and keeps framebuffer compression alive on tilers,
        // storage images need a format with guaranteed storage support though
        ImageDesc mipDesc = {};
        mipDesc.Format = config.UseCompute ? ImageFormat::R16G16B16A16_SFLOAT : ImageFormat::B10G11R11_UFLOAT_PACK32;
        mipDesc.Usage = ImageUsage::SAMPLED | (config.UseCompute ? ImageUsage::STORAGE : ImageUsage::COLOR_ATTACHMENT);
        mipDesc.Sampler = mLinearClampSampler;
        mipDesc.GenerateImGuiID = true;

        mMipSizes.resize(mipCount);
        for(Uint i = 0; i < mipCount; i++)
            mMipSizes[i] = MipSize(config.FullSize, i);

        for(Uint i = 0; i < mipCount; i++)
        {
            mipDesc.Width = mMipSizes[i].x;
            mipDesc.Height = mMipSizes[i].y;
            mipDesc.DebugName = std::format("BloomDown [{}]", i);
            mDownMips.push_back(mRHI->CreateImage(mipDesc));
        }

        for(Uint i = 0; i < mipCount - 1; i++)
        {
            mipDesc.Width = mMipSizes[i].x;
            mipDesc.Height = mMipSizes[i].y;
            if(i == 0)
            {
                mipDesc.DebugName = "Bloom";
                if(mBloomOutput.IsNull())
                    mBloomOutput = mRHI->CreateImage(mipDesc);
                else
                    mRHI->RecreateImage(mBloomOutput, mipDesc);

                mUpMips.push_back(mBloomOutput);
            }
            else
            {
                mipDesc.DebugName = std::format("BloomUp [{}]", i);
                mUpMips.push_back(mRHI->CreateImage(mipDesc));
            }
        }

        if(!config.UseCompute)
        {
            auto createFramebuffer = [this](ImageHandle target, const glm::uvec2& size, const String& debugName) -> FramebufferHandle
            {
                FramebufferAttachment attachment = {};
                attachment.Handle = target;
                attachment.Load = LoadOp::DONT_CARE; // Every texel is overwritten
                attachment.Store = StoreOp::STORE;

                FramebufferDesc fbDesc = {};
                fbDesc.ColorAttachments[0] = attachment;
                fbDesc.ColorAttachmentCount = 1;
                fbDesc.HasDepth = false;
                fbDesc.Width = size.x;
                fbDesc.Height = size.y;
                fbDesc.DebugName = debugName;
                return mRHI->CreateFramebuffer(fbDesc);
            };

            for(Uint i = 0; i < mDownMips.size(); i++)
                mDownFramebuffers.push_back(createFramebuffer(mDownMips[i], mMipSizes[i], std::format("BloomDown [{}] Framebuffer", i)));
            for(Uint i = 0; i < mUpMips.size(); i++)
                mUpFramebuffers.push_back(createFramebuffer(mUpMips[i], mMipSizes[i], std::format("BloomUp [{}] Framebuffer", i)));

            // Every bloom framebuffer shares the same renderpass (same format/load/store), so one pipeline serves all of them
            if(mFragmentPipeline.IsNull())
            {
                PipelineDesc pipelineDesc = {};
                pipelineDesc.Shader_ = Core::GetRenderer()->GetShaderManager().Get("Bloom.glsl");
                pipelineDesc.Raster.Topo = Topology::TRIANGLE_LIST;
                pipelineDesc.Raster.Polygon = PolygonMode::FILL;
                pipelineDesc.Raster.Cull = CullMode::NONE;
                pipelineDesc.Blend.Enable = false;
                pipelineDesc.Depth.TestEnable = false;
                pipelineDesc.Depth.WriteEnable = false;
                pipelineDesc.Stencil.Enable = false;
                pipelineDesc.TargetSwapchain = false;
                pipelineDesc.TargetFramebuffer = mDownFramebuffers[0];
                pipelineDesc.DebugName = "Bloom";
                mFragmentPipeline = mRHI->CreatePipeline(pipelineDesc);
            }
        }

        const PipelineHandle pipeline = config.UseCompute ? mComputePipeline : mFragmentPipeline;
        for(Uint i = 0; i < mDownMips.size(); i++)
            mDownSets.push_back(mRHI->CreateDescriptorSet(pipeline, DescriptorSetSlot::ZERO, DescriptorUpdateFrequency::DYNAMIC, "BloomDown [Set0]"));
        for(Uint i = 0; i < mUpMips.size(); i++)
            mUpSets.push_back(mRHI->CreateDescriptorSet(pipeline, DescriptorSetSlot::ZERO, DescriptorUpdateFrequency::DYNAMIC, "BloomUp [Set0]"));

        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
            mDescriptorsDirty[i] = true;

        // RenderGraph barriers only care about mBloomOutput (read by PostProcess), the rest is listed for the visualizer
        mImageWrites.clear();
        mImageWrites.push_back(mBloomOutput);
        mImageWrites.insert(mImageWrites.end(), mDownMips.begin(), mDownMips.end());
        mImageWrites.insert(mImageWrites.end(), mUpMips.begin() + 1, mUpMips.end());
    }

    void BloomPass::DestroyChain()
    {
        for(DescriptorSetHandle set : mDownSets)
            mRHI->DestroyDescriptorSet(set);
        for(DescriptorSetHandle set : mUpSets)
            mRHI->DestroyDescriptorSet(set);
        for(FramebufferHandle fb : mDownFramebuffers)
            mRHI->DestroyFramebuffer(fb);
        for(FramebufferHandle fb : mUpFramebuffers)
            mRHI->DestroyFramebuffer(fb);
        for(ImageHandle image : mDownMips)
            mRHI->DestroyImage(image);
        for(Uint i = 1; i < mUpMips.size(); i++) // mUpMips[0] is mBloomOutput, recreated in place by BuildChain
            mRHI->DestroyImage(mUpMips[i]);

        mDownSets.clear();
        mUpSets.clear();
        mDownFramebuffers.clear();
        mUpFramebuffers.clear();
        mDownMips.clear();
        mUpMips.clear();
        mMipSizes.clear();
    }

    void BloomPass::Rebuild(const ChainConfig& config)
    {
        mRHI->WaitIdle();
        DestroyChain();
        BuildChain(config);
        mRebuildPending = false;
    }

    void BloomPass::UpdateDescriptorSets(Uint frameIndex)
    {
        // Fragment: 0 = source, 1 = same res mip | Compute: 0 = storage target, 1 = source, 2 = same res mip
        const Uint firstSampledBinding = mConfig.UseCompute ? 1 : 0;

        auto writeSet = [&](DescriptorSetHandle set, ImageHandle target, ImageHandle source, ImageHandle sameRes)
        {
            std::array<DescriptorWrite, 3> writes = {};
            Uint count = 0;
            if(mConfig.UseCompute)
            {
                writes[count].Binding = 0;
                writes[count].Type = DescriptorType::STORAGE_TEXTURE;
                writes[count].Texture = target;
                count++;
            }

            writes[count].Binding = firstSampledBinding;
            writes[count].Type = DescriptorType::TEXTURE;
            writes[count].Texture = source;
            writes[count].Sampler = mLinearClampSampler;
            count++;

            writes[count].Binding = firstSampledBinding + 1;
            writes[count].Type = DescriptorType::TEXTURE;
            writes[count].Texture = sameRes;
            writes[count].Sampler = mLinearClampSampler;
            count++;

            mRHI->UpdateDescriptorSet(set, writes.data(), count, frameIndex);
        };

        // Downsample chain doesn't read a same res mip, bind the source again to keep the descriptor valid
        for(Uint i = 0; i < mDownMips.size(); i++)
        {
            const ImageHandle source = (i == 0) ? mSceneColor : mDownMips[i - 1];
            writeSet(mDownSets[i], mDownMips[i], source, source);
        }

        const Uint lastUp = static_cast<Uint>(mUpMips.size()) - 1;
        for(Uint i = 0; i < mUpMips.size(); i++)
        {
            const ImageHandle lowRes = (i == lastUp) ? mDownMips.back() : mUpMips[i + 1];
            writeSet(mUpSets[i], mUpMips[i], lowRes, mDownMips[i]);
        }

        mDescriptorsDirty[frameIndex] = false;
    }

    void BloomPass::Execute(const FrameContext& ctx, const FrameBlackboard& blackBoard)
    {
        SURGE_PROFILE_FUNC("BloomPass::Execute");
        const BloomSettings& settings = blackBoard.Bloom;
        if(!settings.Enable)
            return;

        // Settings changed (ImGui/gameplay code), rebuild at frame end, this frame still renders with the current chain
        const ChainConfig desired = ComputeConfig(settings);
        if(!(desired == mConfig) && !mRebuildPending)
        {
            mRebuildPending = true;
            Core::AddFrameEndCallback([this, desired]() { Rebuild(desired); });
        }

        if(mDescriptorsDirty[ctx.FrameIndex])
            UpdateDescriptorSets(ctx.FrameIndex);

        const float knee = glm::max(settings.Threshold * settings.Knee, 1e-5f);

        BloomPushConstants pc = {};
        pc.Threshold = glm::vec4(settings.Threshold, settings.Threshold - knee, 2.0f * knee, 0.25f / knee);
        pc.Scatter = glm::clamp(settings.Scatter, 0.0f, 1.0f);
        pc.ClampValue = settings.ClampValue;
        pc.Flags = (settings.HighQualityFilter ? BLOOM_FLAG_HQ_FILTER : 0) | (settings.AntiFlicker ? BLOOM_FLAG_ANTI_FLICKER : 0);

        // Downsample chain
        const ImageDesc& sceneDesc = mRHI->GetDesc(mSceneColor);
        glm::uvec2 sourceSize = { sceneDesc.Width, sceneDesc.Height };
        for(Uint i = 0; i < mDownMips.size(); i++)
        {
            pc.Mode = (i == 0) ? BLOOM_MODE_PREFILTER : BLOOM_MODE_DOWNSAMPLE;
            pc.SourceTexelSize = 1.0f / glm::vec2(sourceSize);
            pc.OutputTexelSize = 1.0f / glm::vec2(mMipSizes[i]);
            RecordStep(ctx, mDownMips[i], mMipSizes[i], mConfig.UseCompute ? FramebufferHandle::Invalid() : mDownFramebuffers[i], mDownSets[i], &pc, sizeof(pc));
            sourceSize = mMipSizes[i];
        }

        // Upsample chain, smallest to largest, ends in mBloomOutput (Up[0])
        pc.Mode = BLOOM_MODE_UPSAMPLE;
        for(int i = static_cast<int>(mUpMips.size()) - 1; i >= 0; i--)
        {
            pc.SourceTexelSize = 1.0f / glm::vec2(mMipSizes[i + 1]);
            pc.OutputTexelSize = 1.0f / glm::vec2(mMipSizes[i]);
            RecordStep(ctx, mUpMips[i], mMipSizes[i], mConfig.UseCompute ? FramebufferHandle::Invalid() : mUpFramebuffers[i], mUpSets[i], &pc, sizeof(pc));
        }
    }

    void BloomPass::RecordStep(const FrameContext& ctx, ImageHandle target, const glm::uvec2& targetSize, FramebufferHandle framebuffer, DescriptorSetHandle set, const void* pushConstants, Uint pushConstantsSize)
    {
        if(mConfig.UseCompute)
        {
            mRHI->CmdTransitionImageLayout(ctx, target, ImageUsage::STORAGE);
            mRHI->CmdBindPipeline(ctx, mComputePipeline);
            mRHI->CmdBindDescriptorSet(ctx, mComputePipeline, set, DescriptorSetSlot::ZERO);
            mRHI->CmdPushConstants(ctx, mComputePipeline, ShaderType::COMPUTE, 0, pushConstantsSize, pushConstants);
            mRHI->CmdDispatch(ctx, (targetSize.x + COMPUTE_GROUP_SIZE - 1) / COMPUTE_GROUP_SIZE, (targetSize.y + COMPUTE_GROUP_SIZE - 1) / COMPUTE_GROUP_SIZE);
        }
        else
        {
            mRHI->CmdBeginRenderPass(ctx, framebuffer);
            mRHI->CmdBindPipeline(ctx, mFragmentPipeline);
            mRHI->CmdBindDescriptorSet(ctx, mFragmentPipeline, set, DescriptorSetSlot::ZERO);
            mRHI->CmdPushConstants(ctx, mFragmentPipeline, ShaderType::VERTEX | ShaderType::FRAGMENT, 0, pushConstantsSize, pushConstants);
            mRHI->CmdDraw(ctx, 3, 1, 0, 0);
            mRHI->CmdEndRenderPass(ctx, framebuffer);
        }

        // Next step (or PostProcess for mBloomOutput) samples it
        mRHI->CmdTransitionImageLayout(ctx, target, ImageUsage::SAMPLED);
    }

    void BloomPass::Resize(Uint width, Uint height, FrameBlackboard& blackBoard)
    {
        if(width == 0 || height == 0)
            return;

        const BloomSettings* settings = &blackBoard.Bloom;
        Core::AddFrameEndCallback([this, width, height, settings]() { mFullSize = { width, height }; Rebuild(ComputeConfig(*settings)); });
    }

    void BloomPass::OnImGuiRender(FrameBlackboard& blackBoard)
    {
        ImFont* boldFont = ImGui::GetIO().Fonts->Fonts[1];
        ImGui::PushFont(boldFont, 25.0f);
        ImGui::TextUnformatted("Bloom");
        ImGui::Separator();
        ImGui::PopFont();

        ImGui::PushID("BloomPass"); // Shares the "Renderer" window with other passes ("Clamp" etc. would collide)
        BloomSettings& settings = blackBoard.Bloom;
        ImGui::Checkbox("Enable Bloom", &settings.Enable);

        constexpr const char* qualityNames[] = { "Low (Mobile)", "Medium", "High", "Ultra (Desktop)" };
        int quality = static_cast<int>(settings.Quality);
        if(ImGui::Combo("Bloom Quality", &quality, qualityNames, IM_ARRAYSIZE(qualityNames)))
            settings.ApplyQualityPreset(static_cast<BloomQuality>(quality));

        ImGui::BeginDisabled(mComputePipeline.IsNull());
        ImGui::Checkbox("Use Compute", &settings.UseCompute);
        ImGui::EndDisabled();
        ImGui::SliderInt("Mip Count", &settings.MipCount, static_cast<int>(MIN_MIP_COUNT), static_cast<int>(MAX_MIP_COUNT));
        ImGui::Checkbox("High Quality Filter (13 tap / Tent)", &settings.HighQualityFilter);
        ImGui::Checkbox("Anti Flicker (Karis Average)", &settings.AntiFlicker);
        ImGui::Checkbox("High Quality Composite", &settings.HighQualityComposite);
        ImGui::Separator();
        ImGui::DragFloat("Threshold", &settings.Threshold, 0.01f, 0.0f, 20.0f, "%.2f");
        ImGui::SliderFloat("Knee", &settings.Knee, 0.0f, 1.0f, "%.2f");
        ImGui::DragFloat("Bloom Intensity", &settings.Intensity, 0.01f, 0.0f, 10.0f, "%.2f");
        ImGui::SliderFloat("Scatter", &settings.Scatter, 0.05f, 0.95f, "%.2f");
        ImGui::DragFloat("Clamp", &settings.ClampValue, 10.0f, 1.0f, 65000.0f, "%.0f");
        ImGui::ColorEdit3("Tint", &settings.Tint.x);

        uint64_t bytes = 0;
        const uint64_t bytesPerPixel = mConfig.UseCompute ? 8 : 4;
        for(Uint i = 0; i < mMipSizes.size(); i++)
            bytes += static_cast<uint64_t>(mMipSizes[i].x) * mMipSizes[i].y * bytesPerPixel * (i + 1 < mMipSizes.size() ? 2 : 1); // Down + Up (no Up for the last mip)

        ImGui::Text("Path: %s | %u mips | %.2f MB", mConfig.UseCompute ? "Compute (RGBA16F)" : "Fragment (R11G11B10)", mConfig.MipCount, static_cast<double>(bytes) / (1024.0 * 1024.0));
        if(!mMipSizes.empty())
            ImGui::Text("Chain: %ux%u -> %ux%u", mMipSizes.front().x, mMipSizes.front().y, mMipSizes.back().x, mMipSizes.back().y);
        ImGui::PopID();
    }

    void BloomPass::Shutdown(FrameBlackboard& /*blackBoard*/)
    {
        DestroyChain();
        mRHI->DestroyImage(mBloomOutput);
        if(!mFragmentPipeline.IsNull())
            mRHI->DestroyPipeline(mFragmentPipeline);
        if(!mComputePipeline.IsNull())
            mRHI->DestroyPipeline(mComputePipeline);
    }
}
