// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Core/Core.hpp"
#include "Surge/Graphics/RenderGraph/Passes/Renderer2DPass.hpp"
#include "Surge/Graphics/RHI/RHI.hpp"
#include "Surge/Graphics//Renderer/Renderer.hpp"

namespace Surge
{
    static constexpr Uint MAX_QUAD_VERTICES = Renderer2DPass::MAX_QUADS_TOTAL * 4;
    static constexpr Uint MAX_QUAD_INDICES = Renderer2DPass::MAX_QUADS_PER_BATCH * 6;// IB of 1 batch, we reuse this

    static constexpr Uint MAX_LINE_VERTICES = Renderer2DPass::MAX_LINES_TOTAL * 2;

    void Renderer2DPass::Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard)
    {
        SURGE_PROFILE_FUNC("Renderer2DPass::Setup");
        mRHI = rhi;

        {
            // Create IB of 1 batch, we resue this for all batches, as the max indices per batch is fixed
            // We fill this with quad index data, and it will reference vertices in the VB based on the current batchs vertex offset
            Vector<Uint> indices(MAX_QUAD_INDICES);
            Uint offset = 0;
            for(Uint i = 0; i < MAX_QUAD_INDICES; i += 6)
            {
                indices[i + 0] = offset + 0;
                indices[i + 1] = offset + 1;
                indices[i + 2] = offset + 2;
                indices[i + 3] = offset + 0;
                indices[i + 4] = offset + 2;
                indices[i + 5] = offset + 3;
                offset += 4;
            }
            BufferDesc ibDesc = {};
            ibDesc.Size = sizeof(Uint) * MAX_QUAD_INDICES;
            ibDesc.Usage = BufferUsage::INDEX;
            ibDesc.HostVisible = false;
            ibDesc.InitialData = indices.data();
            ibDesc.DebugName = "BatchIB";
            mQuadIB = mRHI->CreateBuffer(ibDesc);

            // We allocate a large VBs[RHI_FRAMES_IN_FLIGHT] for max vertices across all batches, but we only fill the portion
            // needed for the current batch each frame. This is to avoid GPU buffer creation stalls when flushing mid-frame after each batch is submitted.
            BufferDesc vbDesc = {};
            vbDesc.Size = sizeof(QuadVertex) * MAX_QUAD_VERTICES;
            vbDesc.Usage = BufferUsage::VERTEX;
            vbDesc.HostVisible = true;
            for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
            {
                vbDesc.DebugName = std::format("BatchVB Frame: {}", i);
                mQuadVB[i] = mRHI->CreateBuffer(vbDesc);
            }

            mCurrentQuadBatch.Reset();
            // CPU side staging array for 1 batch fill this, then memcpy-ied to GPU buffer
            mCurrentQuadBatch.VertexData.resize(MAX_QUADS_PER_BATCH * 4);

            PipelineDesc desc = {};
            desc.Shader_ = Core::GetRenderer()->GetShaderManager().Get("Renderer2D.glsl");
            desc.Raster.Cull = CullMode::NONE;
            desc.DebugName = "Renderer2D";
            desc.TargetFramebuffer = blackBoard.MainPassFramebuffer;
            desc.TargetSwapchain = false;
            desc.Blend.Enable = false;
            desc.Depth.TestEnable = true;
            desc.Depth.WriteEnable = true;
            desc.Depth.Op = CompareOp::LESS_OR_EQUAL;
            m2DPipeline = mRHI->CreatePipeline(desc);

            // Textures TODO
            //for (Uint i = 0; i < MAX_BATCHES_PER_FRAME; i++)
            //    mTexDescriptorSets[i] = mRHI->CreateDescriptorSet(m2DPipeline, 1, DescriptorUpdateFrequency::DYNAMIC, "Renderer2D_TexDescriptorSet");

            // Frame UBO
            mFrameDescriptorSet = mRHI->CreateDescriptorSet(m2DPipeline, DescriptorSetSlot::ZERO, DescriptorUpdateFrequency::DYNAMIC, "2D_FrameData [Set0]");

            for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
            {
                DescriptorWrite write = {};
                write.Binding = 0;
                write.Type = DescriptorType::UNIFORM_BUFFER;
                write.Buffer = blackBoard.FrameUBOs[i];
                write.BufferOffset = 0;
                write.BufferRange = sizeof(FrameUBO);
                mRHI->UpdateDescriptorSet(mFrameDescriptorSet, &write, 1, i);
            }
            mImageWrites.push_back(blackBoard.MainPassColorImage);
        }
        {
            ////////////////////////////////////// Lines //////////////////////////////////////
            PipelineDesc desc = {};
            desc.Shader_ = Core::GetRenderer()->GetShaderManager().Get("Renderer2DLine.glsl");
            desc.Raster.Cull = CullMode::NONE;
            desc.Raster.Topo = Topology::LINE_LIST;
            desc.Raster.LineWidth = 2.0f;
            desc.DebugName = "Renderer2D_Line";
            desc.TargetFramebuffer = blackBoard.MainPassFramebuffer;
            desc.TargetSwapchain = false;
            desc.Blend.Enable = false;
            desc.Depth.TestEnable = true;
            desc.Depth.WriteEnable = true;
            desc.Depth.Op = CompareOp::LESS_OR_EQUAL;
            m2DLinePipeline = mRHI->CreatePipeline(desc);

            BufferDesc vbDesc = {};
            vbDesc.Size = sizeof(LineVertex) * MAX_LINE_VERTICES;
            vbDesc.Usage = BufferUsage::VERTEX;
            vbDesc.HostVisible = true;
            for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
            {
                vbDesc.DebugName = std::format("BatchLineVB Frame: {}", i);
                mLineVB[i] = mRHI->CreateBuffer(vbDesc);
            }
            mCurrentLineBatch.VertexData.resize(MAX_LINES_PER_BATCH * 2);
        }
    }

    void Renderer2DPass::Execute(const FrameContext& ctx, const FrameBlackboard& blackboard)
    {
        SURGE_PROFILE_FUNC("Renderer2DPass::Execute");
        mCurrentFrameCtx = ctx;

        // QuadReset
        mQuadDrawCommandCount = 0;
        mTotalQuadlVertexCount = 0;
        mTotalQuadCount = 0;
        mCurrentFrameVertexOffset = 0;

        for(const QuadSubmitCmd& quad : blackboard.QuadList)
        {
            if(mCurrentQuadBatch.QuadCount >= MAX_QUADS_PER_BATCH)
                RegisterQuadDrawcall();

            if(mTotalQuadCount == MAX_QUADS_TOTAL)
            {
                Log<Severity::Warn>("Max Quads per frame reached!");
                mMaxQuadCountReached = true;
                break;
            }
            mMaxQuadCountReached = false;
            Uint texIndex = 0;

            static constexpr glm::vec4 sLocalPositions[4] = {
                { 0.5f, -0.5f, 0.0f, 1.0f},
                { 0.5f,  0.5f, 0.0f, 1.0f},
                {-0.5f,  0.5f, 0.0f, 1.0f},
                {-0.5f, -0.5f, 0.0f, 1.0f},
            };
            static constexpr glm::vec2 sUVs[4] = {
                {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f}
            };

            const Uint packedColor = glm::packUnorm4x8(quad.Color);
            for(Uint i = 0; i < 4; i++)
            {
                QuadVertex& v = mCurrentQuadBatch.VertexData[mCurrentQuadBatch.VertexCount++];
                v.Position = quad.Transform * sLocalPositions[i];
                v.Color = packedColor;
                v.UV = sUVs[i];
                v.TextureIndex = texIndex;
            }

            mCurrentQuadBatch.QuadCount++;
            mTotalQuadCount++;
        }
        RegisterQuadDrawcall();

        if(!mQuadDrawCommands.empty())
        {
            mRHI->CmdBindPipeline(mCurrentFrameCtx, m2DPipeline);
            mRHI->CmdBindDescriptorSet(mCurrentFrameCtx, m2DPipeline, mFrameDescriptorSet, DescriptorSetSlot::ZERO);
            mRHI->CmdBindVertexBuffer(mCurrentFrameCtx, mQuadVB[mCurrentFrameCtx.FrameIndex], 0);
            mRHI->CmdBindIndexBuffer(mCurrentFrameCtx, mQuadIB, 0);

            for(const QuadDrawCmd& cmd : mQuadDrawCommands)
                mRHI->CmdDrawIndexed(mCurrentFrameCtx, cmd.QuadCount * 6, 1, 0, (int32_t)cmd.VertexOffset, 0);
        }

        //
        ////////////////////////////////////// Lines //////////////////////////////////////
        //

        // Reset
        mLineDrawCommandCount = 0;
        mTotalLineVertexCount = 0;
        mTotalLineCount = 0;
        mCurrentLineVertexOffset = 0;
        mMaxLinesCountReached = false;
        for(const LineSubmitCmd& line : blackboard.LineList)
        {
            if(mCurrentLineBatch.LineCount >= MAX_LINES_PER_BATCH)
                RegisterLineDrawcall();

            if(mTotalLineCount == MAX_LINES_TOTAL)
            {
                Log<Severity::Warn>("Max Line per frame reached!");
                mMaxLinesCountReached = true;
                break;
            }

            mCurrentLineBatch.VertexData[mCurrentLineBatch.VertexCount++] = LineVertex { line.P0, line.Color };
            mCurrentLineBatch.VertexData[mCurrentLineBatch.VertexCount++] = LineVertex { line.P1, line.Color };
            mCurrentLineBatch.LineCount++;
            mTotalLineCount++;
        }

        if(mCurrentLineBatch.LineCount > 0)
            RegisterLineDrawcall();

        if(!mLineDrawCommands.empty())
        {
            mRHI->CmdBindPipeline(ctx, m2DLinePipeline);
            mRHI->CmdBindDescriptorSet(ctx, m2DLinePipeline, mFrameDescriptorSet, DescriptorSetSlot::ZERO);
            mRHI->CmdBindVertexBuffer(ctx, mLineVB[ctx.FrameIndex], 0);

            for(const auto& cmd : mLineDrawCommands)
                mRHI->CmdDraw(ctx, cmd.VertexCount, 1, cmd.VertexOffset, 0);
        }
    }

    void Renderer2DPass::RegisterQuadDrawcall()
    {
        if(mCurrentQuadBatch.QuadCount == 0)
            return;

        const FrameContext& ctx = mCurrentFrameCtx;

        // Upload current batch vertex data to GPU (only the portion needed for this batch, not the entire VB)
        Uint uploadOffsetInBytes = mCurrentFrameVertexOffset * sizeof(QuadVertex);
        Uint uploadSizeInBytes = mCurrentQuadBatch.VertexCount * sizeof(QuadVertex);
        mRHI->UploadBuffer(mQuadVB[ctx.FrameIndex], mCurrentQuadBatch.VertexData.data(), uploadSizeInBytes, uploadOffsetInBytes);

        mQuadDrawCommands[mQuadDrawCommandCount++] = { mCurrentFrameVertexOffset, mCurrentQuadBatch.QuadCount };

        mTotalQuadlVertexCount += mCurrentQuadBatch.VertexCount;
        mCurrentFrameVertexOffset += mCurrentQuadBatch.VertexCount;
        mCurrentQuadBatch.Reset();
    }

    void Renderer2DPass::RegisterLineDrawcall()
    {
        Uint uploadOffsetInBytes = mCurrentLineVertexOffset * sizeof(LineVertex);
        Uint uploadSizeInBytes = mCurrentLineBatch.VertexCount * sizeof(LineVertex);
        mRHI->UploadBuffer(mLineVB[mCurrentFrameCtx.FrameIndex], mCurrentLineBatch.VertexData.data(), uploadSizeInBytes, uploadOffsetInBytes);

        mLineDrawCommands[mLineDrawCommandCount++] = { mCurrentLineVertexOffset, mCurrentLineBatch.VertexCount };

        mTotalLineVertexCount += mCurrentLineBatch.VertexCount;
        mCurrentLineVertexOffset += mCurrentLineBatch.VertexCount;
        mCurrentLineBatch.Reset();
    }

    void Renderer2DPass::Resize(Uint width, Uint height, FrameBlackboard& blackBoard)
    {
        // We do nothing here, as Renderer2DPass writes to framebuffer of GeometryPass...
        // GeometryPass resizes the framebuffer
    }

    void Renderer2DPass::Shutdown(FrameBlackboard& blackBoard)
    {
        SURGE_PROFILE_FUNC("Renderer2DPass::Shutdown");
        mRHI->DestroyDescriptorSet(mFrameDescriptorSet);
        mRHI->DestroyPipeline(m2DLinePipeline);
        mRHI->DestroyPipeline(m2DPipeline);

        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
            mRHI->DestroyBuffer(mQuadVB[i]);

        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
            mRHI->DestroyBuffer(mLineVB[i]);

        mRHI->DestroyBuffer(mQuadIB);
    }

    void Renderer2DPass::OnImGuiRender(FrameBlackboard& blackBoard)
    {
        ImFont* boldFont = ImGui::GetIO().Fonts->Fonts[1];
        ImGui::PushFont(boldFont, 25.0f);
        ImGui::TextUnformatted("Renderer2D");
        ImGui::Separator();
        ImGui::PopFont();

        ImGui::PushFont(boldFont, 20.0f);
        ImGui::TextUnformatted("Quads");
        ImGui::Separator();
        ImGui::PopFont();

        {
            ImGui::Text("Batches: %u / %u", (mTotalQuadCount + MAX_QUADS_PER_BATCH - 1) / MAX_QUADS_PER_BATCH, (MAX_QUADS_TOTAL + MAX_QUADS_PER_BATCH - 1) / MAX_QUADS_PER_BATCH);

            if(mMaxQuadCountReached)
            {
                ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.25f, 1.0f), "Max quad count of Renderer2D reached!\nSome Quads were not rendered.");
                ImGui::Separator();
            }

            float totalVerticesUsed = (float)mTotalQuadlVertexCount;
            float usageRatio = totalVerticesUsed / MAX_QUAD_VERTICES;

            float frameWeightMB = (sizeof(QuadVertex) * totalVerticesUsed) / 1024.0f / 1024.0f;
            float totalWeightMB = (sizeof(QuadVertex) * MAX_QUAD_VERTICES) / 1024.0f / 1024.0f;
            ImGui::Text("Vertex Buffer(GPU): %.1f MB / %.1f MB", frameWeightMB, totalWeightMB);

            ImVec4 barColor = ImVec4(0.0f, 0.7f, 0.0f, 1.0f); // Green
            if(usageRatio > 0.65f) barColor = ImVec4(1.0f, 0.4f, 0.0f, 1.0f); // Orange
            if(usageRatio > 0.95f) barColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Red

            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, barColor);
            ImGui::ProgressBar(usageRatio, ImVec2(-1.0f, 0.0f));
            ImGui::Text("%u / %u Vertices", mTotalQuadlVertexCount, MAX_QUAD_VERTICES);
            ImGui::PopStyleColor();
        }

        ImGui::Separator();
        ImGui::PushFont(boldFont, 20.0f);
        ImGui::TextUnformatted("Lines");
        ImGui::Separator();
        ImGui::PopFont();
        {
            ImGui::Text("Batches: %u / %u", (mTotalLineCount + MAX_LINES_PER_BATCH - 1) / MAX_LINES_PER_BATCH, (MAX_LINES_TOTAL + MAX_LINES_PER_BATCH - 1) / MAX_LINES_PER_BATCH);

            if(mMaxLinesCountReached)
            {
                ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.25f, 1.0f), "Max Line count of Renderer2D reached!");
                ImGui::Separator();
            }

            float totalVerticesUsed = (float)mTotalLineVertexCount;
            float usageRatio = totalVerticesUsed / MAX_LINE_VERTICES;

            float frameWeightMB = (sizeof(LineVertex) * totalVerticesUsed) / 1024.0f / 1024.0f;
            float totalWeightMB = (sizeof(LineVertex) * MAX_LINE_VERTICES) / 1024.0f / 1024.0f;
            ImGui::Text("Vertex Buffer(GPU): %.1f MB / %.1f MB", frameWeightMB, totalWeightMB);

            ImVec4 barColor = ImVec4(0.0f, 0.7f, 0.0f, 1.0f); // Green
            if(usageRatio > 0.65f) barColor = ImVec4(1.0f, 0.4f, 0.0f, 1.0f); // Orange
            if(usageRatio > 0.95f) barColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Red

            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, barColor);
            ImGui::ProgressBar(usageRatio, ImVec2(-1.0f, 0.0f));
            ImGui::Text("%u / %u Vertices", mTotalLineVertexCount, MAX_LINE_VERTICES);
            ImGui::PopStyleColor();
        }
    }
}
