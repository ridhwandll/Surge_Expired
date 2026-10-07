// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RenderGraph/RenderPass.hpp"

namespace Surge
{
    /*
    * BloomPass:
    * Reads : BlackBoard.MainPassColorImage
    * Writes: BlackBoard.BloomImage (+ internal mip chain)
    *
    * Mip chain at 1/2, 1/4, 1/8 ... resolution (manages its own renderpasses/dispatches, like ShadowPass):
    *   Prefilter : SceneColor   -> Down[0]   (anti flicker + soft threshold)
    *   Downsample: Down[i - 1]  -> Down[i]
    *   Upsample  : Up[i + 1] (or Down[last]) + Down[i] -> Up[i]   (Up[0] == BlackBoard.BloomImage)
    * PostProcess upscales BloomImage and adds it to the HDR color before tonemapping
    *
    * Two backends selected by BlackBoard.Bloom.UseCompute:
    *   Fragment (mobile) : renderpass per mip, R11G11B10 color attachments
    *   Compute (desktop) : dispatch per mip, RGBA16F storage images
    */

    class GraphicsRHI;
    class BloomPass : public RenderPass
    {
    public:
        BloomPass() { mName = "BloomPass"; mGroup = PassGroup::BLOOM; }
        virtual ~BloomPass() = default;

        void Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard) override;
        void Execute(const FrameContext& ctx, const FrameBlackboard& blackBoard) override;
        void Resize(Uint width, Uint height, FrameBlackboard& blackBoard) override;
        void OnImGuiRender(FrameBlackboard& blackBoard) override;
        void Shutdown(FrameBlackboard& blackBoard) override;

    private:
        struct ChainConfig
        {
            bool UseCompute = false;
            Uint MipCount = 0;
            glm::uvec2 FullSize = { 0, 0 };

            bool operator==(const ChainConfig&) const = default;
        };

        ChainConfig ComputeConfig(const BloomSettings& settings) const;
        void BuildChain(const ChainConfig& config);
        void DestroyChain();
        void Rebuild(const ChainConfig& config);
        void UpdateDescriptorSets(Uint frameIndex);
        void RecordStep(const FrameContext& ctx, ImageHandle target, const glm::uvec2& targetSize, FramebufferHandle framebuffer, DescriptorSetHandle set, const void* pushConstants, Uint pushConstantsSize);

    private:
        GraphicsRHI* mRHI = nullptr;

        ImageHandle mSceneColor;
        ImageHandle mBloomOutput; // == mUpMips[0], stable handle (RecreateImage) so the RenderGraph barriers stay valid
        SamplerHandle mLinearClampSampler;

        PipelineHandle mFragmentPipeline; // Created lazily on first fragment build, R11G11B10 renderpass never changes
        PipelineHandle mComputePipeline;  // Null when the device has no compute support

        // Rebuilt on resize / config change
        glm::uvec2 mFullSize = { 1, 1 };
        ChainConfig mConfig;
        Vector<glm::uvec2> mMipSizes;
        Vector<ImageHandle> mDownMips;
        Vector<ImageHandle> mUpMips; // MipCount - 1 entries, last level reads Down[last] directly
        Vector<FramebufferHandle> mDownFramebuffers; // Fragment path only
        Vector<FramebufferHandle> mUpFramebuffers;   // Fragment path only
        Vector<DescriptorSetHandle> mDownSets;
        Vector<DescriptorSetHandle> mUpSets;

        bool mRebuildPending = false;
        bool mDescriptorsDirty[RHISettings::FRAMES_IN_FLIGHT] = {};
    };
}
