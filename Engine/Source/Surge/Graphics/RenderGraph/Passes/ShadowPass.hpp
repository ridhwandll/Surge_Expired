// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RenderGraph/RenderPass.hpp"

namespace Surge
{
    /*
    * ShadowPass:
    * Reads : Nothing
    * Writes: Blackboard.ShadowAtlas, Blackboard.ShadowUBOs
    *
    * Directional light cascaded shadow maps. Every cascade is a region of one D16 atlas (side by side), all of them are drawn
    * in a single render pass with a viewport per cascade. Prepare fits the cascades (stable bounding spheres, texel snapping,
    * near plane fitted to the casters), culls + instances the casters per cascade and uploads the ShadowUBO. Skinned casters are
    * instanced the same way into a second buffer and drawn with ShadowSkinned.glsl inside the same render pass
    */
    class GraphicsRHI;
    class ShadowPass : public RenderPass
    {
    public:
        ShadowPass() { mName = "ShadowPass"; mGroup = PassGroup::SHADOW; }
        virtual ~ShadowPass() = default;

        void Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard) override;
        void Prepare(const FrameContext& ctx, FrameBlackboard& blackBoard) override;
        void Execute(const FrameContext& ctx, const FrameBlackboard& blackBoard) override;
        bool HasWork(const FrameBlackboard& blackBoard) const override { return blackBoard.ShadowSettings_.Enable && blackBoard.HasDirectionalLight; }
        void Resize(Uint width, Uint height, FrameBlackboard& blackBoard) override;
        void RenderStatsViaImGui(FrameBlackboard& blackBoard) override;
        void Shutdown(FrameBlackboard& blackBoard) override;

    private:
        struct Caster
        {
            glm::mat4 Transform;
            glm::vec3 Center;  // World space AABB
            glm::vec3 Extents;
            const Mesh* Mesh_;
            Uint SubmeshIndex;
            Uint SkinOffset;   // INVALID_SKIN_OFFSET for static casters
        };

        struct ShadowDrawItem
        {
            const Mesh* Mesh_;
            Uint SubmeshIndex;
            Uint CasterIndex;
            bool Skinned;
        };

        struct SkinnedInstance // Mirrored in ShadowSkinned.glsl
        {
            glm::mat4 Transform;
            Uint SkinOffset;
            Uint _pad[3];
        };
        static_assert(sizeof(SkinnedInstance) == 80, "SkinnedInstance is mirrored by ShadowSkinned.glsl (std430)");

        struct ShadowDraw
        {
            const Mesh* Mesh_;
            Uint SubmeshIndex;
            Uint FirstInstance;
            Uint InstanceCount;
        };

        Uint ComputeResolution(Uint requested, Uint cascadeCount) const;
        void UpdateAtlas(FrameBlackboard& blackBoard, Uint cascadeCount);
        void GatherCasters(const FrameBlackboard& blackBoard);
        void BuildCascades(const FrameBlackboard& blackBoard, Uint cascadeCount);
        void UploadShadowData(const FrameContext& ctx, const FrameBlackboard& blackBoard, Uint cascadeCount);
        void EnsureInstanceCapacity(Uint frameIndex, Uint instanceCount);
        void EnsureSkinnedInstanceCapacity(Uint frameIndex, Uint instanceCount);
        void UpdateSkinDescriptors(const FrameContext& ctx, const FrameBlackboard& blackBoard);

    private:
        GraphicsRHI* mRHI;

        // One framebuffer + pipeline for the whole atlas, the atlas keeps its handle when recreated (RenderGraph barriers stay valid)
        FramebufferHandle mFramebuffer;
        PipelineHandle mPipeline;
        Uint mResolution = 0;    // Per cascade, after clamping to the device's max image size
        Uint mAtlasCascades = 0; // Cascades the atlas is laid out for
        Uint mActiveCascades = 0; // Cascades rendered this frame, 0 = nothing to do

        std::array<glm::mat4, MAX_SHADOW_CASCADE_COUNT> mLightViewProjections = {};
        std::array<float, MAX_SHADOW_CASCADE_COUNT> mCascadeEnds = {};
        std::array<float, MAX_SHADOW_CASCADE_COUNT> mTexelWorldSizes = {};
        float mShadowDistance = 0.0f;

        // (Rid) Instancing: every cascade's visible casters, sorted by mesh, back to back in one per-frame buffer
        BufferHandle mInstanceBuffers[RHISettings::FRAMES_IN_FLIGHT];
        Uint mInstanceCapacity[RHISettings::FRAMES_IN_FLIGHT] = {};
        DescriptorSetHandle mInstanceSet;
        Vector<Caster> mCasters;           // Every shadow casting submesh this frame
        Vector<ShadowDrawItem> mDrawItems; // Visible items of one cascade
        Vector<glm::mat4> mInstanceData;   // What goes into the instance buffer
        std::array<Vector<ShadowDraw>, MAX_SHADOW_CASCADE_COUNT> mCascadeDraws;
        std::array<Uint, MAX_SHADOW_CASCADE_COUNT> mCascadeVisible = {};

        // Skinned casters
        PipelineHandle mSkinnedPipeline;
        DescriptorSetHandle mSkinnedInstanceSet; // Skinned instances + skin matrices
        BufferHandle mSkinnedInstanceBuffers[RHISettings::FRAMES_IN_FLIGHT];
        Uint mSkinnedInstanceCapacity[RHISettings::FRAMES_IN_FLIGHT] = {};
        Uint mSkinBufferVersion[RHISettings::FRAMES_IN_FLIGHT] = {};
        Vector<SkinnedInstance> mSkinnedInstanceData;
        std::array<Vector<ShadowDraw>, MAX_SHADOW_CASCADE_COUNT> mCascadeSkinnedDraws;
    };
}
