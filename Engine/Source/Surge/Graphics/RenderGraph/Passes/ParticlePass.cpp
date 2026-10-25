// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Core/Profiler.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Graphics/RenderGraph/Passes/ParticlePass.hpp"
#include <imgui.h>
#include <cfloat>
#include <numeric>

namespace Surge
{
    static constexpr Uint MIN_RECORD_CAPACITY = 4096; // 128 KB per frame in flight, grows on demand
    static constexpr Uint MIN_EMITTER_CAPACITY = 64;
    static constexpr Uint TEXTURE_SET_EVICTION_INTERVAL = 240; // Frames between sweeps of descriptor sets whose texture was destroyed

    void ParticlePass::Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard)
    {
        SURGE_PROFILE_FUNC("ParticlePass::Setup");
        mRHI = rhi;

        PipelineDesc desc = {};
        desc.Shader_ = mContext.Shaders->Get("Particle.glsl");
        desc.Raster.Cull = CullMode::NONE;
        desc.Depth.TestEnable = true;
        desc.Depth.WriteEnable = false;
        desc.Depth.Op = CompareOp::LESS_OR_EQUAL;

        // Premultiplied alpha: the shader outputs (rgb * a, a * (1 - additive)), so alpha blended and additive particles share it
        desc.Blend.Enable = true;
        desc.Blend.SrcColor = BlendFactor::ONE;
        desc.Blend.DstColor = BlendFactor::ONE_MINUS_SRC_ALPHA;
        desc.Blend.ColorOp = BlendOp::ADD;
        desc.Blend.SrcAlpha = BlendFactor::ONE;
        desc.Blend.DstAlpha = BlendFactor::ONE_MINUS_SRC_ALPHA;
        desc.Blend.AlphaOp = BlendOp::ADD;

        desc.TargetFramebuffer = blackBoard.MainPassFramebuffer;
        desc.TargetSwapchain = false;
        desc.DebugName = "Particles";
        mPipeline = mRHI->CreatePipeline(desc);

        SamplerDesc samplerDesc = {};
        samplerDesc.Min = FilterMode::LINEAR;
        samplerDesc.Mag = FilterMode::LINEAR;
        samplerDesc.Mip = MipmapMode::LINEAR;
        samplerDesc.WrapU = WrapMode::CLAMP; // Flipbook cells must not bleed into each other
        samplerDesc.WrapV = WrapMode::CLAMP;
        samplerDesc.DebugName = "ParticleSampler";
        mSampler = mRHI->CreateSampler(samplerDesc);

        mDefaultTexture = blackBoard.ParticleDefaultImage;

        mFrameDescriptorSet = mRHI->CreateDescriptorSet(mPipeline, DescriptorSetSlot::ZERO, DescriptorUpdateFrequency::DYNAMIC, "Particles_FrameData [Set0]");
        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
        {
            DescriptorWrite write = {};
            write.Binding = 0;
            write.Type = DescriptorType::UNIFORM_BUFFER;
            write.Buffer = blackBoard.FrameUBOs[i];
            write.BufferRange = sizeof(FrameUBO);
            mRHI->UpdateDescriptorSet(mFrameDescriptorSet, &write, 1, i);

            EnsureCapacity(mRecordBuffers, mRecordCapacity, i, MIN_RECORD_CAPACITY, sizeof(ParticleRecord), 1, "Particles_Records");
            EnsureCapacity(mEmitterBuffers, mEmitterCapacity, i, MIN_EMITTER_CAPACITY, sizeof(EmitterGPU), 2, "Particles_Emitters");
        }

        mImageWrites.push_back(blackBoard.MainPassColorImage);
    }

    void ParticlePass::EnsureCapacity(BufferHandle (&buffers)[RHISettings::FRAMES_IN_FLIGHT], Uint (&capacities)[RHISettings::FRAMES_IN_FLIGHT],
                                      Uint frameIndex, Uint required, Uint stride, Uint binding, const char* debugName)
    {
        if(required <= capacities[frameIndex] && !buffers[frameIndex].IsNull())
            return;

        Uint capacity = std::max(capacities[frameIndex], 1u);
        while(capacity < required)
            capacity *= 2;

        if(!buffers[frameIndex].IsNull())
            mRHI->DestroyBuffer(buffers[frameIndex]); // Deferred, the previous use of this frame slot has already retired

        BufferDesc desc = {};
        desc.Usage = BufferUsage::STORAGE;
        desc.HostVisible = true;
        desc.Size = static_cast<uint64_t>(stride) * capacity;
        desc.DebugName = std::format("{} Frame: {}", debugName, frameIndex);
        buffers[frameIndex] = mRHI->CreateBuffer(desc);
        capacities[frameIndex] = capacity;

        // Only this frame's copy of the set changes, before it is bound this frame
        DescriptorWrite write = {};
        write.Binding = binding;
        write.Type = DescriptorType::STORAGE_BUFFER;
        write.Buffer = buffers[frameIndex];
        write.BufferRange = desc.Size;
        mRHI->UpdateDescriptorSet(mFrameDescriptorSet, &write, 1, frameIndex);
    }

    DescriptorSetHandle ParticlePass::GetTextureSet(ImageHandle image)
    {
        const uint64_t key = (static_cast<uint64_t>(image.Generation) << 32) | image.Index;
        auto it = mTextureSets.find(key);
        if(it != mTextureSets.end())
            return it->second.Data2;

        const DescriptorSetHandle set = mRHI->CreateDescriptorSet(mPipeline, DescriptorSetSlot::ONE, DescriptorUpdateFrequency::DYNAMIC, "Particles_Texture [Set1]");
        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
        {
            DescriptorWrite write = {};
            write.Binding = 0;
            write.Type = DescriptorType::TEXTURE;
            write.Texture = image;
            write.Sampler = mSampler;
            mRHI->UpdateDescriptorSet(set, &write, 1, i);
        }

        mTextureSets.emplace(key, Pair<ImageHandle, DescriptorSetHandle> { image, set });
        return set;
    }

    void ParticlePass::SortByDistance(const FrameBlackboard& blackBoard, const ParticleEmitterCmd& emitter)
    {
        const ParticleRecord* records = blackBoard.ParticleList.data() + emitter.FirstRecord;
        const Uint count = emitter.RecordCount;
        const glm::vec3 cameraPosition = blackBoard.CameraPosition;
        const glm::vec3 forward = -glm::vec3(blackBoard.ViewMatrix[0][2], blackBoard.ViewMatrix[1][2], blackBoard.ViewMatrix[2][2]);

        float nearest = FLT_MAX;
        float farthest = -FLT_MAX;
        mSortKeys.resize(count);
        mSortTemp.resize(count);
        mSortDepths.resize(count);
        for(Uint i = 0; i < count; i++)
        {
            const float depth = glm::dot(records[i].Position - cameraPosition, forward);
            nearest = std::min(nearest, depth);
            farthest = std::max(farthest, depth);
            mSortDepths[i] = depth;
        }

        // 16 bit keys, 0 = farthest: back to front is ascending
        const float range = farthest - nearest;
        const float scale = range > 1e-6f ? 65535.0f / range : 0.0f;
        for(Uint i = 0; i < count; i++)
            mSortKeys[i] = { static_cast<uint16_t>((farthest - mSortDepths[i]) * scale), i };

        // LSD radix sort, two 8 bit passes, stable
        for(Uint shift = 0; shift < 16; shift += 8)
        {
            std::array<Uint, 257> offsets = {};
            for(const Pair<uint16_t, Uint>& key : mSortKeys)
                offsets[((key.Data1 >> shift) & 0xFF) + 1]++;
            for(Uint b = 1; b < offsets.size(); b++)
                offsets[b] += offsets[b - 1];
            for(const Pair<uint16_t, Uint>& key : mSortKeys)
                mSortTemp[offsets[(key.Data1 >> shift) & 0xFF]++] = key;
            std::swap(mSortKeys, mSortTemp);
        }

        mSortedRecords.resize(count);
        for(Uint i = 0; i < count; i++)
            mSortedRecords[i] = records[mSortKeys[i].Data2];
        mSortedParticles += count;
    }

    void ParticlePass::Prepare(const FrameContext& ctx, FrameBlackboard& blackBoard)
    {
        SURGE_PROFILE_FUNC("ParticlePass::Prepare");
        mDraws.clear();
        mDrawnParticles = 0;
        mDrawnEmitters = 0;
        mSortedParticles = 0;

        // Descriptor sets of destroyed textures (hot reloads, unloaded assets) are released now and then
        if(++mFramesSinceEviction >= TEXTURE_SET_EVICTION_INTERVAL)
        {
            mFramesSinceEviction = 0;
            for(auto it = mTextureSets.begin(); it != mTextureSets.end();)
            {
                if(!mRHI->IsValid(it->second.Data1))
                {
                    mRHI->DestroyDescriptorSet(it->second.Data2);
                    it = mTextureSets.erase(it);
                }
                else
                    ++it;
            }
        }

        const Vector<ParticleEmitterCmd>& emitters = blackBoard.ParticleEmitterList;
        if(emitters.empty())
            return;

        mProjectionScaleY = std::abs(blackBoard.ProjectionMatrix[1][1]);

        // Emitter parameters, indexed by the emitter index stored in every record (submission order)
        const Uint emitterCount = static_cast<Uint>(emitters.size());
        mEmitterData.resize(emitterCount);
        for(Uint i = 0; i < emitterCount; i++)
        {
            const ParticleEmitterCmd& cmd = emitters[i];
            EmitterGPU& gpu = mEmitterData[i];
            gpu.Mode = static_cast<Uint>(cmd.Mode);
            gpu.Columns = std::max(cmd.FlipbookColumns, 1u);
            gpu.Rows = std::max(cmd.FlipbookRows, 1u);
            gpu.FrameBlending = cmd.FrameBlending ? 1u : 0u;
            gpu.AdditiveAmount = cmd.AdditiveAmount;
            gpu.EmissiveIntensity = cmd.EmissiveIntensity;
            gpu.MaxScreenSize = cmd.MaxScreenSize;
            gpu.StretchSpeedScale = cmd.StretchSpeedScale;
            gpu.StretchLengthScale = cmd.StretchLengthScale;
            gpu._pad0 = gpu._pad1 = gpu._pad2 = 0.0f;
        }
        EnsureCapacity(mEmitterBuffers, mEmitterCapacity, ctx.FrameIndex, emitterCount, sizeof(EmitterGPU), 2, "Particles_Emitters");
        mRHI->UploadBuffer(mEmitterBuffers[ctx.FrameIndex], mEmitterData.data(), static_cast<Uint>(sizeof(EmitterGPU) * emitterCount), 0);

        // Back to front between emitters: SortingOrder first, then the distance of their bounds along the view direction
        const glm::vec3 cameraPosition = blackBoard.CameraPosition;
        const glm::vec3 forward = -glm::vec3(blackBoard.ViewMatrix[0][2], blackBoard.ViewMatrix[1][2], blackBoard.ViewMatrix[2][2]);
        mEmitterOrder.resize(emitterCount);
        std::iota(mEmitterOrder.begin(), mEmitterOrder.end(), 0u);
        std::sort(mEmitterOrder.begin(), mEmitterOrder.end(), [&](Uint a, Uint b) {
            const ParticleEmitterCmd& ea = emitters[a];
            const ParticleEmitterCmd& eb = emitters[b];
            if(ea.SortingOrder != eb.SortingOrder)
                return ea.SortingOrder < eb.SortingOrder;
            const float da = glm::dot(ea.BoundsCenter - cameraPosition, forward);
            const float db = glm::dot(eb.BoundsCenter - cameraPosition, forward);
            if(da != db)
                return da > db;
            return a < b;
        });

        // Records in draw order, consecutive emitters with the same texture become one draw
        const Uint totalRecords = static_cast<Uint>(blackBoard.ParticleList.size());
        EnsureCapacity(mRecordBuffers, mRecordCapacity, ctx.FrameIndex, totalRecords, sizeof(ParticleRecord), 1, "Particles_Records");

        Uint offset = 0;
        for(Uint emitterIndex : mEmitterOrder)
        {
            const ParticleEmitterCmd& cmd = emitters[emitterIndex];
            if(cmd.RecordCount == 0)
                continue;

            const ParticleRecord* source = blackBoard.ParticleList.data() + cmd.FirstRecord;
            if(cmd.Sort == ParticleSortMode::DISTANCE && cmd.RecordCount > 1)
            {
                SortByDistance(blackBoard, cmd);
                source = mSortedRecords.data();
            }
            mRHI->UploadBuffer(mRecordBuffers[ctx.FrameIndex], source, static_cast<Uint>(sizeof(ParticleRecord) * cmd.RecordCount), static_cast<Uint>(sizeof(ParticleRecord) * offset));

            const ImageHandle texture = (!cmd.Texture.IsNull() && mRHI->IsValid(cmd.Texture)) ? cmd.Texture : mDefaultTexture;
            const DescriptorSetHandle textureSet = GetTextureSet(texture);
            if(!mDraws.empty() && mDraws.back().TextureSet == textureSet)
                mDraws.back().ParticleCount += cmd.RecordCount;
            else
                mDraws.push_back({ textureSet, offset, cmd.RecordCount });

            offset += cmd.RecordCount;
            mDrawnEmitters++;
        }
        mDrawnParticles = offset;
    }

    void ParticlePass::Execute(const FrameContext& ctx, const FrameBlackboard& blackBoard)
    {
        SURGE_PROFILE_FUNC("ParticlePass::Execute");
        (void)blackBoard;

        if(mDraws.empty())
            return;

        mRHI->CmdBindPipeline(ctx, mPipeline);
        mRHI->CmdBindDescriptorSet(ctx, mPipeline, mFrameDescriptorSet, DescriptorSetSlot::ZERO);

        PushConstantData pushConstants = {};
        pushConstants.ProjectionScaleY = mProjectionScaleY;
        mRHI->CmdPushConstants(ctx, mPipeline, ShaderType::VERTEX | ShaderType::FRAGMENT, 0, sizeof(PushConstantData), &pushConstants);

        for(const DrawCmd& draw : mDraws)
        {
            mRHI->CmdBindDescriptorSet(ctx, mPipeline, draw.TextureSet, DescriptorSetSlot::ONE);
            mRHI->CmdDraw(ctx, draw.ParticleCount * 6, 1, draw.FirstParticle * 6, 0); // 6 vertices per particle, no vertex/index buffer
        }
    }

    void ParticlePass::Resize([[maybe_unused]] Uint width, [[maybe_unused]] Uint height, [[maybe_unused]] FrameBlackboard& blackBoard)
    {
        // Nothing size dependent, the pipeline follows the main pass framebuffer
    }

    void ParticlePass::RenderStatsViaImGui(FrameBlackboard& blackBoard)
    {
        const ParticleStats& stats = blackBoard.ParticleStatistics;
        if(stats.Systems == 0 && mDrawnParticles == 0)
            return;

        ImGui::SeparatorText("Particles");
        ImGui::Text("Particles: %u drawn / %u alive (budget %u)", mDrawnParticles, stats.AliveParticles, blackBoard.Particles.MaxParticles);
        ImGui::Text("Systems: %u (%u culled)", stats.Systems, stats.CulledSystems);
        ImGui::Text("Emitters: %u, draw calls: %u", mDrawnEmitters, static_cast<Uint>(mDraws.size()));
        if(mSortedParticles > 0)
            ImGui::Text("Distance sorted: %u", mSortedParticles);
    }

    void ParticlePass::Shutdown([[maybe_unused]] FrameBlackboard& blackBoard)
    {
        SURGE_PROFILE_FUNC("ParticlePass::Shutdown");
        for(auto& [key, entry] : mTextureSets)
            mRHI->DestroyDescriptorSet(entry.Data2);
        mTextureSets.clear();

        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
        {
            mRHI->DestroyBuffer(mRecordBuffers[i]);
            mRHI->DestroyBuffer(mEmitterBuffers[i]);
        }

        mRHI->DestroyDescriptorSet(mFrameDescriptorSet);
        mRHI->DestroySampler(mSampler);
        mRHI->DestroyPipeline(mPipeline);
    }
}
