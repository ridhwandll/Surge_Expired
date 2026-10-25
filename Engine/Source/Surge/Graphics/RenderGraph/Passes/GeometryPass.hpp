// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RenderGraph/RenderPass.hpp"
#include "Surge/Graphics/RHI/RHIHandle.hpp"

namespace Surge
{
    /*
    * GeometryPass:
    * Reads : BlackBoard.ShadowAtlas
    * Writes: Blackboard.MainPassColorImage, BlackBoard.MainPassDepthImage
    *
    * Per frame (Prepare): frustum culls every submitted submesh, sorts the survivors by material/mesh and merges identical
    * (material, mesh, submesh) runs into one instanced draw. Per-instance transforms live in a per-frame storage buffer
    * indexed with gl_InstanceIndex (Renderer3D.glsl). Skinned meshes get the same treatment in a second list drawn with
    * Renderer3DSkinned.glsl: each instance also carries its palette offset, so copies of one character stay one draw per submesh
    */
    class GraphicsRHI;
    class Material;
    class GeometryPass : public RenderPass
    {
    public:
        GeometryPass() { mName = "GeometryPass"; mGroup = PassGroup::MAIN_SCENE; }
        virtual ~GeometryPass() = default;

        void Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard) override;
        void Prepare(const FrameContext& ctx, FrameBlackboard& blackBoard) override;
        void Execute(const FrameContext& ctx, const FrameBlackboard& blackBoard) override;
        void Resize(Uint width, Uint height, FrameBlackboard& blackBoard) override;
        void RenderStatsViaImGui(FrameBlackboard& blackBoard) override;
        void Shutdown(FrameBlackboard& blackBoard) override;

    public:
        static constexpr Uint MAX_LIGHTS = 64; // Size of the Lights[] array in Renderer3DFragment.glsli, the per-platform budget is FrameBlackboard::Lighting

    private:
        struct PushConstantData
        {
            Uint LightCount;
        };

        struct DrawItem
        {
            const Material* Material_;
            const Mesh* Mesh_;
            Uint SubmeshIndex;
            Uint TransformIndex; // Into mVisibleTransforms
        };

        struct InstancedDraw
        {
            const Material* Material_;
            const Mesh* Mesh_;
            Uint SubmeshIndex;
            Uint FirstInstance;
            Uint InstanceCount;
        };

        struct SkinnedInstance // Mirrored in Renderer3DSkinned.glsl
        {
            glm::mat4 Transform;
            Uint SkinOffset;
            Uint _pad[3];
        };
        static_assert(sizeof(SkinnedInstance) == 80, "SkinnedInstance is mirrored by Renderer3DSkinned.glsl (std430)");

        void BuildDrawList(const FrameContext& ctx, const FrameBlackboard& blackBoard);
        void BuildSkinnedDrawList(const FrameContext& ctx, const FrameBlackboard& blackBoard);
        void EnsureSkinnedInstanceCapacity(Uint frameIndex, Uint instanceCount);
        void UpdateSkinDescriptors(const FrameContext& ctx, const FrameBlackboard& blackBoard);
        void DrawRuns(const FrameContext& ctx, PipelineHandle pipeline, const Vector<InstancedDraw>& draws);
        void UploadLights(const FrameContext& ctx, const FrameBlackboard& blackBoard);
        void EnsureInstanceCapacity(Uint frameIndex, Uint instanceCount);
        void UpdateDepthStoreOp(const FrameBlackboard& blackBoard);
        void UpdateShadowDescriptors(const FrameContext& ctx, const FrameBlackboard& blackBoard);

    private:
        GraphicsRHI* mRHI;
        DescriptorSetHandle mFrameDescriptorSet;
        DescriptorSetHandle mShadowMapDescriptorSet;
        SamplerHandle mShadowSampler;
        Uint mShadowAtlasVersion[RHISettings::FRAMES_IN_FLIGHT] = {}; // FrameBlackboard::ShadowAtlasVersion each frame's set was written with
        BufferHandle mLightUBOs[RHISettings::FRAMES_IN_FLIGHT];
        PipelineHandle m3DPipeline;

        // Instancing
        BufferHandle mInstanceBuffers[RHISettings::FRAMES_IN_FLIGHT];
        Uint mInstanceCapacity[RHISettings::FRAMES_IN_FLIGHT] = {};
        Vector<glm::mat4> mVisibleTransforms;
        Vector<glm::mat4> mSortedTransforms;
        Vector<DrawItem> mDrawItems;
        Vector<InstancedDraw> mDraws;

        // Skinned meshes
        PipelineHandle m3DSkinnedPipeline;
        DescriptorSetHandle mSkinnedFrameDescriptorSet; // Frame UBO + lights + skinned instances + skin matrices
        BufferHandle mSkinnedInstanceBuffers[RHISettings::FRAMES_IN_FLIGHT];
        Uint mSkinnedInstanceCapacity[RHISettings::FRAMES_IN_FLIGHT] = {};
        Uint mSkinBufferVersion[RHISettings::FRAMES_IN_FLIGHT] = {}; // FrameBlackboard::SkinBufferVersion each frame's set was written with
        Vector<SkinnedInstance> mVisibleSkinned;
        Vector<SkinnedInstance> mSortedSkinned;
        Vector<DrawItem> mSkinnedDrawItems;
        Vector<InstancedDraw> mSkinnedDraws;

        // Lights (scratch, reused every frame)
        LightUBOData mLightData = {};
        Vector<Pair<float, Uint>> mPointLightOrder;
        Uint mUploadedLightCount = 0;

        bool mDepthStored = true; // Depth is only written back to memory when something samples it later (Example: AO)

        // Stats
        Uint mSubmittedSubmeshes = 0;
        Uint mVisibleSubmeshes = 0;
        Uint mSubmittedSkinned = 0;
        Uint mVisibleSkinnedMeshes = 0;
    };
}
