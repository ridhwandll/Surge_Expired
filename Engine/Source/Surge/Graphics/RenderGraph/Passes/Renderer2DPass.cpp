// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Core/Profiler.hpp"
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

            // Quad/text vertex buffers (one per frame in flight) are created on first use and grow with demand, see EnsureVertexCapacity()

            mCurrentQuadBatch.Reset();
            // CPU side staging array for 1 batch fill this, then memcpy-ied to GPU buffer
            mCurrentQuadBatch.VertexData.resize(MAX_QUADS_PER_BATCH * 4);

            PipelineDesc desc = {};
            desc.Shader_ = mContext.Shaders->Get("Renderer2D.glsl");
            desc.Raster.Cull = CullMode::NONE;
            desc.DebugName = "Renderer2D";
            desc.TargetFramebuffer = blackBoard.MainPassFramebuffer;
            desc.TargetSwapchain = false;
            desc.Blend.Enable = true;
            desc.Depth.TestEnable = true;
            desc.Depth.WriteEnable = true;
            desc.Depth.Op = CompareOp::LESS_OR_EQUAL;
            m2DQuadPipeline = mRHI->CreatePipeline(desc);

            // Frame UBO
            mFrameDescriptorSet = mRHI->CreateDescriptorSet(m2DQuadPipeline, DescriptorSetSlot::ZERO, DescriptorUpdateFrequency::DYNAMIC, "2D_FrameData [Set0]");
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

            // (Rid) Textures descriptor sets for each batch, with MAX_TEX_SLOTS_PER_BATCH slots each. We update the texture slots dynamically per batch as needed
            mTexDescriptorSets.resize(MAX_QUAD_BATCHES_PER_FRAME);
            for(Uint i = 0; i < MAX_QUAD_BATCHES_PER_FRAME; i++)
            {
                mTexDescriptorSets[i] = mRHI->CreateDescriptorSet(m2DQuadPipeline, DescriptorSetSlot::ONE, DescriptorUpdateFrequency::DYNAMIC, std::format("Renderer2D_TexDescriptorSet_{}", i).c_str());
                for(Uint frame = 0; frame < RHISettings::FRAMES_IN_FLIGHT; frame++)
                {
                    for(Uint slot = 0; slot < MAX_TEX_SLOTS_PER_BATCH; slot++)
                    {
                        DescriptorWrite write = {};
                        write.Binding = 0;
                        write.Type = DescriptorType::TEXTURE;
                        write.ArrayIndex = slot;
                        write.Texture = blackBoard.WhiteImage;
                        write.Sampler = blackBoard.DefaultSampler;
                        mRHI->UpdateDescriptorSet(mTexDescriptorSets[i], &write, 1, frame);
                    }
                }
            }
        }
        {
            ////////////////////////////////////// Lines //////////////////////////////////////
            PipelineDesc desc = {};
            desc.Shader_ = mContext.Shaders->Get("Renderer2DLine.glsl");
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

            // Line vertex buffers are created on first use, see EnsureVertexCapacity()
            mCurrentLineBatch.VertexData.resize(MAX_LINES_PER_BATCH * 2);
        }
        {
            ////////////////////////////////////// Text //////////////////////////////////////
            PipelineDesc desc = {};
            desc.Shader_ = mContext.Shaders->Get("Renderer2DText.glsl");
            desc.Raster.Cull = CullMode::NONE;
            desc.DebugName = "Renderer2D_Text";
            desc.TargetFramebuffer = blackBoard.MainPassFramebuffer;
            desc.TargetSwapchain = false;
            desc.Blend.Enable = true;
            desc.Depth.TestEnable = true;
            desc.Depth.WriteEnable = true;
            desc.Depth.Op = CompareOp::LESS_OR_EQUAL;
            m2DTextPipeline = mRHI->CreatePipeline(desc);
        }
        mImageWrites.push_back(blackBoard.MainPassColorImage);
    }

    void Renderer2DPass::EnsureVertexCapacity(BufferHandle& buffer, Uint& capacity, Uint requiredVertices, Uint stride, const char* debugName, Uint frameIndex)
    {
        if(requiredVertices == 0 || requiredVertices <= capacity)
            return;

        Uint newCapacity = std::max(capacity, MIN_VERTEX_CAPACITY);
        while(newCapacity < requiredVertices)
            newCapacity *= 2;

        if(!buffer.IsNull())
            mRHI->DestroyBuffer(buffer); // Deferred, this frame slot's previous use has retired

        BufferDesc vbDesc = {};
        vbDesc.Size = stride * newCapacity;
        vbDesc.Usage = BufferUsage::VERTEX;
        vbDesc.HostVisible = true;
        vbDesc.DebugName = std::format("{} Frame: {}", debugName, frameIndex);
        buffer = mRHI->CreateBuffer(vbDesc);
        capacity = newCapacity;
    }

    void Renderer2DPass::Execute(const FrameContext& ctx, const FrameBlackboard& blackboard)
    {
        SURGE_PROFILE_FUNC("Renderer2DPass::Execute");
        mCurrentFrameCtx = ctx;

        // Size this frame's vertex buffers up front (quads and text share mQuadVB), the draws bind them once after all batches are written
        {
            Uint quadVertices = static_cast<Uint>(blackboard.QuadList.size()) * 4;
            for(const TextSubmitCmd& txt : blackboard.TextList)
                quadVertices += static_cast<Uint>(txt.Text.size()) * 4 * ((txt.EnableShadow && txt.ShadowColor.a > 0.001f) ? 2 : 1); // Upper bound, every char a glyph
            quadVertices = std::min(quadVertices, MAX_QUAD_VERTICES);
            EnsureVertexCapacity(mQuadVB[ctx.FrameIndex], mQuadVBCapacity[ctx.FrameIndex], quadVertices, sizeof(QuadVertex), "BatchVB", ctx.FrameIndex);

            const Uint lineVertices = std::min(static_cast<Uint>(blackboard.LineList.size()) * 2, MAX_LINE_VERTICES);
            EnsureVertexCapacity(mLineVB[ctx.FrameIndex], mLineVBCapacity[ctx.FrameIndex], lineVertices, sizeof(LineVertex), "BatchLineVB", ctx.FrameIndex);
        }

        // ==========================================================================
        // QUADS
        // ==========================================================================
        mQuadBatchCount = 0;
        mTotalQuadlVertexCount = 0;
        mTotalQuadCount = 0;
        mCurrentFrameVertexOffset = 0;
        mMaxQuadCountReached = false;

        for(const QuadSubmitCmd& quad : blackboard.QuadList)
        {
            if(mCurrentQuadBatch.QuadCount >= MAX_QUADS_PER_BATCH)
                FlushQuadBatch();

            if(mQuadBatchCount >= MAX_QUAD_BATCHES_PER_FRAME || mTotalQuadCount == MAX_QUADS_TOTAL)
            {
                Log<Severity::Warn>("Max Quads/Batches per frame reached!");
                mMaxQuadCountReached = true;
                break;
            }

            int slot = mCurrentQuadBatch.FindOrAssignTextureSlot(quad.Texture);
            if(slot == QuadBatchData::MAX_TEX_IN_BATCH_REACHED)
            {
                FlushQuadBatch();
                if(mQuadBatchCount >= MAX_QUAD_BATCHES_PER_FRAME) break;
                slot = mCurrentQuadBatch.FindOrAssignTextureSlot(quad.Texture);
            }

            const Uint packedColor = glm::packUnorm4x8(quad.Color);
            static constexpr glm::vec2 sUVs[4] = { { 1.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 1.0f } };
            if(quad.Billboard)
            {
                // Extract position and scale from the transform matrix, throw away rotation
                glm::vec3 position = glm::vec3(quad.Transform[3]);
                float scaleX = glm::length(glm::vec3(quad.Transform[0]));
                float scaleY = glm::length(glm::vec3(quad.Transform[1]));

                float halfW = scaleX * 0.5f;
                float halfH = scaleY * 0.5f;
                glm::vec3 positions[4] = {
                    position + (blackboard.CameraRight * halfW) - (blackboard.CameraUp * halfH), // Bottom-Right Corner
                    position + (blackboard.CameraRight * halfW) + (blackboard.CameraUp * halfH), // Top-Right Corner
                    position - (blackboard.CameraRight * halfW) + (blackboard.CameraUp * halfH), // Top-Left Corner
                    position - (blackboard.CameraRight * halfW) - (blackboard.CameraUp * halfH)  // Bottom-Left Corner
                };

                for(Uint i = 0; i < 4; i++)
                {
                    QuadVertex& v = mCurrentQuadBatch.VertexData[mCurrentQuadBatch.VertexCount++];
                    v.Position = positions[i];
                    v.Color = packedColor;
                    v.UV = sUVs[i];
                    v.TextureIndex = (Uint)slot;
                }
            }
            else
            {
                static constexpr glm::vec4 sLocalPositions[4] = { { 0.5f, -0.5f, 0.0f, 1.0f}, { 0.5f,  0.5f, 0.0f, 1.0f}, {-0.5f,  0.5f, 0.0f, 1.0f}, {-0.5f, -0.5f, 0.0f, 1.0f} };
                for(Uint i = 0; i < 4; i++)
                {
                    QuadVertex& v = mCurrentQuadBatch.VertexData[mCurrentQuadBatch.VertexCount++];
                    v.Position = quad.Transform * sLocalPositions[i];
                    v.Color = packedColor;
                    v.UV = sUVs[i];
                    v.TextureIndex = (Uint)slot;
                }
            }

            mCurrentQuadBatch.QuadCount++;
            mTotalQuadCount++;
        }

        if(mCurrentQuadBatch.QuadCount > 0)
            FlushQuadBatch();

        if(mQuadBatchCount > 0)
        {
            mRHI->CmdBindPipeline(mCurrentFrameCtx, m2DQuadPipeline);
            mRHI->CmdBindDescriptorSet(mCurrentFrameCtx, m2DQuadPipeline, mFrameDescriptorSet, DescriptorSetSlot::ZERO);
            mRHI->CmdBindVertexBuffer(mCurrentFrameCtx, mQuadVB[mCurrentFrameCtx.FrameIndex], 0);
            mRHI->CmdBindIndexBuffer(mCurrentFrameCtx, mQuadIB, 0);

            for(Uint i = 0; i < mQuadBatchCount; i++)
            {
                const QuadDrawCmd& cmd = mQuadDrawCommands[i];
                mRHI->CmdBindDescriptorSet(mCurrentFrameCtx, m2DQuadPipeline, mTexDescriptorSets[i], DescriptorSetSlot::ONE);
                mRHI->CmdDrawIndexed(mCurrentFrameCtx, cmd.QuadCount * 6, 1, 0, (int32_t)cmd.VertexOffset, 0);
            }
        }

        // ==========================================================================
        // LINES
        // ==========================================================================
        mLineBatchCount = 0;
        mTotalLineVertexCount = 0;
        mTotalLineCount = 0;
        mCurrentLineVertexOffset = 0;
        mMaxLinesCountReached = false;

        for(const LineSubmitCmd& line : blackboard.LineList)
        {
            if(mCurrentLineBatch.LineCount >= MAX_LINES_PER_BATCH)
                FlushLineBatch();

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
            FlushLineBatch();

        if(mLineBatchCount > 0)
        {
            mRHI->CmdBindPipeline(ctx, m2DLinePipeline);
            mRHI->CmdBindDescriptorSet(ctx, m2DLinePipeline, mFrameDescriptorSet, DescriptorSetSlot::ZERO);
            mRHI->CmdBindVertexBuffer(ctx, mLineVB[ctx.FrameIndex], 0);

            for(Uint i = 0; i < mLineBatchCount; i++)
            {
                const LineDrawCmd& cmd = mLineDrawCommands[i];
                mRHI->CmdDraw(ctx, cmd.VertexCount, 1, cmd.VertexOffset, 0);
            }
        }

        // ==========================================================================
        // TEXT
        // ==========================================================================
        Uint textBatchStartIndex = mQuadBatchCount; // Record where the text batches begin

        struct TextPushConstants
        {
            float PxRange;
            float pad[2];
        };

        std::array<TextPushConstants, MAX_QUAD_BATCHES_PER_FRAME> textBatchParams;
        TextPushConstants currentTextParams = { 8.0f, {0.0f, 0.0f} };

        for(const TextSubmitCmd& txt : blackboard.TextList)
        {
            if(!txt.FontAsset || txt.Text.empty() || mMaxQuadCountReached)
                continue;

            // BILLBOARD
            glm::mat4 activeTransform = txt.Transform;
            if(txt.Billboard)
            {
                glm::vec3 pos = glm::vec3(txt.Transform[3]);
                float scaleX = glm::length(glm::vec3(txt.Transform[0]));
                float scaleY = glm::length(glm::vec3(txt.Transform[1]));
                float scaleZ = glm::length(glm::vec3(txt.Transform[2]));

                glm::vec3 forward = glm::normalize(glm::cross(blackboard.CameraRight, blackboard.CameraUp));
                activeTransform = glm::mat4(
                    glm::vec4(blackboard.CameraRight * scaleX, 0.0f),
                    glm::vec4(blackboard.CameraUp * scaleY, 0.0f),
                    glm::vec4(forward * scaleZ, 0.0f),
                    glm::vec4(pos, 1.0f)
                );
            }

            ImageHandle fontAtlas = txt.FontAsset->GetAtlas();

            const Uint packedColorText = glm::packUnorm4x8(txt.Color);
            const Uint packedColorShadow = glm::packUnorm4x8(txt.ShadowColor);

            glm::vec4 localPositions[4];
            glm::vec2 uvs[4];

            // LINE MEASUREMENT
            mLineLayoutCache.clear();
            float currentLineWidth = 0.0f;
            Uint measurePrevChar = 0;

            for(size_t i = 0; i < txt.Text.size(); i++)
            {
                char c = txt.Text[i];

                if(txt.MaxWidth > 0.0f && c == ' ')
                {
                    float nextWordLength = 0.0f;
                    for(size_t j = i + 1; j < txt.Text.size() && txt.Text[j] != ' ' && txt.Text[j] != '\n'; j++)
                    {
                        const FontGlyph* nextGlyph = txt.FontAsset->GetGlyph(txt.Text[j]);
                        if(nextGlyph) nextWordLength += nextGlyph->Advance;
                    }

                    if(currentLineWidth + nextWordLength > txt.MaxWidth)
                    {
                        mLineLayoutCache.push_back(currentLineWidth);
                        currentLineWidth = 0.0f;
                        measurePrevChar = 0;
                        continue;
                    }
                }
                if(c == '\n')
                {
                    mLineLayoutCache.push_back(currentLineWidth);
                    currentLineWidth = 0.0f;
                    measurePrevChar = 0;
                    continue;
                }

                const FontGlyph* glyph = txt.FontAsset->GetGlyph(c);
                if(!glyph) continue;

                if(measurePrevChar != 0)
                    currentLineWidth += txt.FontAsset->GetKerning(measurePrevChar, c);

                currentLineWidth += (glyph->Advance + txt.LetterSpacing);
                measurePrevChar = c;
            }
            mLineLayoutCache.push_back(currentLineWidth); // Push the final line width

            // CursorX
            auto getCursorX = [&](Uint lineIdx) -> float {
                if(lineIdx >= mLineLayoutCache.size()) [[unlikely]] return 0.0f;

                float lineWidth = mLineLayoutCache[lineIdx];
                if(txt.Alignment == TextAlignment::CENTER)
                    return -lineWidth * 0.5f;
                else if(txt.Alignment == TextAlignment::RIGHT)
                    return -lineWidth;
                return 0.0f; // Left alignment
            };

            // TODO:
            // Add Background for text
            // Strikethrough
            // Rich Text

            // GEOMETRY GENERATION
            auto buildTextQuads = [&](glm::vec2 posOffset, float zOffset, Uint packedColor) {

                currentTextParams.PxRange = txt.FontAsset->GetPxRange();
                textBatchParams[mQuadBatchCount] = currentTextParams;

                Uint lineIndex = 0;
                Uint drawPrevChar = 0;
                float italicSkew = txt.Italic ? 0.25f : 0.0f;

                float capHeight = txt.FontAsset->GetLineHeight() * 0.7f;
                float descent = txt.FontAsset->GetLineHeight() * 0.2f;

                Uint numLines = (Uint)mLineLayoutCache.size();
                float blockDrop = numLines > 0 ? ((numLines - 1) * (txt.FontAsset->GetLineHeight() + txt.LineSpacing)) : 0.0f;

                float cursorY = 0.0f; // Default BASELINE
                if(txt.VerticalAlignment == TextVerticalAlignment::CENTER)
                    cursorY = (blockDrop + descent - capHeight) * 0.5f;
                else if(txt.VerticalAlignment == TextVerticalAlignment::TOP)
                    cursorY = -capHeight;
                else if(txt.VerticalAlignment == TextVerticalAlignment::BOTTOM)
                    cursorY = blockDrop + descent;

                float cursorX = getCursorX(lineIndex);

                auto drawUnderline = [&]() {
                    float lineW = mLineLayoutCache[lineIndex];
                    float thickness = txt.FontAsset->GetLineHeight() * 0.06f; // 6% of line height
                    float yOffset = txt.FontAsset->GetLineHeight() * 0.15f;   // 15% of line height below baseline

                    glm::vec2 uMin = { cursorX + posOffset.x, cursorY - yOffset - thickness + posOffset.y };
                    glm::vec2 uMax = { cursorX + lineW + posOffset.x, cursorY - yOffset + posOffset.y };

                    localPositions[0] = { uMax.x, uMin.y, zOffset, 1.0f };
                    localPositions[1] = { uMax.x, uMax.y, zOffset, 1.0f };
                    localPositions[2] = { uMin.x, uMax.y, zOffset, 1.0f };
                    localPositions[3] = { uMin.x, uMin.y, zOffset, 1.0f };

                    uvs[0] = { 1.0f, 1.0f }; uvs[1] = { 1.0f, 0.0f }; uvs[2] = { 0.0f, 0.0f }; uvs[3] = { 0.0f, 1.0f };

                    for(int vIdx = 0; vIdx < 4; vIdx++)
                    {
                        QuadVertex& v = mCurrentQuadBatch.VertexData[mCurrentQuadBatch.VertexCount++];
                        v.Position = activeTransform * localPositions[vIdx];
                        v.Color = packedColor; v.UV = uvs[vIdx]; v.TextureIndex = 0; // White Texture
                    }
                    mCurrentQuadBatch.QuadCount++; mTotalQuadCount++;
                };

                auto nextLine = [&]() {
                    cursorY -= (txt.FontAsset->GetLineHeight() + txt.LineSpacing);
                    lineIndex++;
                    cursorX = getCursorX(lineIndex);
                    drawPrevChar = 0;
                    if(txt.Underline && lineIndex < mLineLayoutCache.size())
                        drawUnderline();
                };

                if(txt.Underline)
                    drawUnderline();

                for(size_t i = 0; i < txt.Text.size(); i++)
                {
                    if(mTotalQuadCount >= MAX_QUADS_TOTAL)
                    {
                        Log<Severity::Warn>("Max Quads per frame reached during Text Pass!");
                        mMaxQuadCountReached = true;
                        break;
                    }

                    char c = txt.Text[i];

                    if(txt.MaxWidth > 0.0f && c == ' ')
                    {
                        float nextWordLength = 0.0f;
                        for(size_t j = i + 1; j < txt.Text.size() && txt.Text[j] != ' ' && txt.Text[j] != '\n'; j++)
                        {
                            const FontGlyph* nextGlyph = txt.FontAsset->GetGlyph(txt.Text[j]);
                            if(nextGlyph) nextWordLength += nextGlyph->Advance;
                        }

                        float baseWidth = txt.Alignment == TextAlignment::LEFT ? cursorX : cursorX + (txt.Alignment == TextAlignment::CENTER ? mLineLayoutCache[lineIndex] * 0.5f : mLineLayoutCache[lineIndex]);
                        if(baseWidth + nextWordLength > txt.MaxWidth)
                        {
                            nextLine();
                            continue;
                        }
                    }

                    if(c == '\n')
                    {
                        nextLine();
                        continue;
                    }

                    const FontGlyph* glyph = txt.FontAsset->GetGlyph(c);
                    if(!glyph)
                        continue;

                    if(drawPrevChar != 0)
                        cursorX += txt.FontAsset->GetKerning(drawPrevChar, c);

                    int slot = mCurrentQuadBatch.FindOrAssignTextureSlot(fontAtlas);
                    if(slot == QuadBatchData::MAX_TEX_IN_BATCH_REACHED || mCurrentQuadBatch.QuadCount >= MAX_QUADS_PER_BATCH)
                    {
                        textBatchParams[mQuadBatchCount] = currentTextParams;
                        FlushQuadBatch();
                        if(mTotalQuadCount == MAX_QUADS_TOTAL || mQuadBatchCount >= MAX_QUAD_BATCHES_PER_FRAME)
                        {
                            mMaxQuadCountReached = true;
                            break;
                        }
                        slot = mCurrentQuadBatch.FindOrAssignTextureSlot(fontAtlas);
                        textBatchParams[mQuadBatchCount] = currentTextParams;
                    }

                    glm::vec2 quadMin = glm::vec2(cursorX + glyph->PlaneBounds[0].x + posOffset.x, cursorY + glyph->PlaneBounds[0].y + posOffset.y);
                    glm::vec2 quadMax = glm::vec2(cursorX + glyph->PlaneBounds[1].x + posOffset.x, cursorY + glyph->PlaneBounds[1].y + posOffset.y);

                    localPositions[0] = { quadMax.x + (glyph->PlaneBounds[0].y * italicSkew), quadMin.y, zOffset, 1.0f };
                    localPositions[1] = { quadMax.x + (glyph->PlaneBounds[1].y * italicSkew), quadMax.y, zOffset, 1.0f };
                    localPositions[2] = { quadMin.x + (glyph->PlaneBounds[1].y * italicSkew), quadMax.y, zOffset, 1.0f };
                    localPositions[3] = { quadMin.x + (glyph->PlaneBounds[0].y * italicSkew), quadMin.y, zOffset, 1.0f };

                    // (Rid) MSDF JSON assumes Y goes Up, but Vulkan assumes Y goes Down. Invert Y here
                    uvs[0] = { glyph->UVBounds[1].x, 1.0f - glyph->UVBounds[0].y };
                    uvs[1] = { glyph->UVBounds[1].x, 1.0f - glyph->UVBounds[1].y };
                    uvs[2] = { glyph->UVBounds[0].x, 1.0f - glyph->UVBounds[1].y };
                    uvs[3] = { glyph->UVBounds[0].x, 1.0f - glyph->UVBounds[0].y };

                    for(Uint vIdx = 0; vIdx < 4; vIdx++)
                    {
                        QuadVertex& v = mCurrentQuadBatch.VertexData[mCurrentQuadBatch.VertexCount++];
                        v.Position = activeTransform * localPositions[vIdx];
                        v.Color = packedColor;
                        v.UV = uvs[vIdx];
                        v.TextureIndex = (Uint)slot;
                    }

                    cursorX += (glyph->Advance + txt.LetterSpacing);
                    drawPrevChar = c;

                    mCurrentQuadBatch.QuadCount++;
                    mTotalQuadCount++;
                }
            }; // End Geometry Generation Lambda

            if(txt.EnableShadow && txt.ShadowColor.a > 0.001f)
                buildTextQuads(txt.ShadowOffset, -0.01f, packedColorShadow);

            // Main Text
            buildTextQuads({ 0.0f, 0.0f }, 0.0f, packedColorText);
        }

        if(mCurrentQuadBatch.QuadCount > 0)
        {
            textBatchParams[mQuadBatchCount] = currentTextParams;
            FlushQuadBatch();
        }

        if(mQuadBatchCount > textBatchStartIndex)
        {
            mRHI->CmdBindPipeline(mCurrentFrameCtx, m2DTextPipeline);
            mRHI->CmdBindDescriptorSet(mCurrentFrameCtx, m2DTextPipeline, mFrameDescriptorSet, DescriptorSetSlot::ZERO);
            mRHI->CmdBindVertexBuffer(mCurrentFrameCtx, mQuadVB[mCurrentFrameCtx.FrameIndex], 0);
            mRHI->CmdBindIndexBuffer(mCurrentFrameCtx, mQuadIB, 0);

            for(Uint i = textBatchStartIndex; i < mQuadBatchCount; i++)
            {
                const TextPushConstants& params = textBatchParams[i];
                mRHI->CmdPushConstants(mCurrentFrameCtx, m2DTextPipeline, ShaderType::VERTEX | ShaderType::FRAGMENT, 0, sizeof(TextPushConstants), &params);

                const QuadDrawCmd& cmd = mQuadDrawCommands[i];
                mRHI->CmdBindDescriptorSet(mCurrentFrameCtx, m2DTextPipeline, mTexDescriptorSets[i], DescriptorSetSlot::ONE);
                mRHI->CmdDrawIndexed(mCurrentFrameCtx, cmd.QuadCount * 6, 1, 0, (int32_t)cmd.VertexOffset, 0);
            }
        }
    }

    void Renderer2DPass::FlushQuadBatch()
    {
        if(mCurrentQuadBatch.QuadCount == 0)
            return;

        if(mQuadBatchCount >= MAX_QUAD_BATCHES_PER_FRAME)
        {
            Log<Severity::Error>("Renderer2DPass: Exceeded max Quad batches!");
            return;
        }

        const FrameContext& ctx = mCurrentFrameCtx;
        const Uint batchIndex = mQuadBatchCount;

        for(Uint slot = 0; slot < mCurrentQuadBatch.TextureCount; slot++)
        {
            if (mCurrentQuadBatch.Textures[slot].IsNull())
                continue;

            DescriptorWrite write = {};
            write.Binding = 0;
            write.Type = DescriptorType::TEXTURE;
            write.ArrayIndex = slot;
            write.Texture = mCurrentQuadBatch.Textures[slot];
            write.Sampler = mContext.TextSampler; // TODO: DO NOT HARDCODE Text Sampler always, expose Sampler as asset
            mRHI->UpdateDescriptorSet(mTexDescriptorSets[batchIndex], &write, 1, ctx.FrameIndex);
        }

        if(mCurrentFrameVertexOffset + mCurrentQuadBatch.VertexCount > mQuadVBCapacity[ctx.FrameIndex])
        {
            Log<Severity::Error>("Renderer2DPass: quad vertex buffer overflow, batch dropped!");
            mCurrentQuadBatch.Reset();
            return;
        }

        const Uint uploadOffsetInBytes = mCurrentFrameVertexOffset * sizeof(QuadVertex);
        const Uint uploadSizeInBytes = mCurrentQuadBatch.VertexCount * sizeof(QuadVertex);
        mRHI->UploadBuffer(mQuadVB[ctx.FrameIndex], mCurrentQuadBatch.VertexData.data(), uploadSizeInBytes, uploadOffsetInBytes);

        mQuadDrawCommands[mQuadBatchCount++] = { mCurrentFrameVertexOffset, mCurrentQuadBatch.QuadCount };

        mTotalQuadlVertexCount += mCurrentQuadBatch.VertexCount;
        mCurrentFrameVertexOffset += mCurrentQuadBatch.VertexCount;
        mCurrentQuadBatch.Reset();
    }

    void Renderer2DPass::FlushLineBatch()
    {
        if(mCurrentLineVertexOffset + mCurrentLineBatch.VertexCount > mLineVBCapacity[mCurrentFrameCtx.FrameIndex])
        {
            Log<Severity::Error>("Renderer2DPass: line vertex buffer overflow, batch dropped!");
            mCurrentLineBatch.Reset();
            return;
        }

        const Uint uploadOffsetInBytes = mCurrentLineVertexOffset * sizeof(LineVertex);
        const Uint uploadSizeInBytes = mCurrentLineBatch.VertexCount * sizeof(LineVertex);
        mRHI->UploadBuffer(mLineVB[mCurrentFrameCtx.FrameIndex], mCurrentLineBatch.VertexData.data(), uploadSizeInBytes, uploadOffsetInBytes);

        mLineDrawCommands[mLineBatchCount++] = { mCurrentLineVertexOffset, mCurrentLineBatch.VertexCount };

        mTotalLineVertexCount += mCurrentLineBatch.VertexCount;
        mCurrentLineVertexOffset += mCurrentLineBatch.VertexCount;
        mCurrentLineBatch.Reset();
    }

    void Renderer2DPass::Resize(Uint, Uint, FrameBlackboard&)
    {
        // We do nothing here, as Renderer2DPass writes to framebuffer of GeometryPass...
        // GeometryPass resizes the framebuffer
    }

    void Renderer2DPass::Shutdown(FrameBlackboard&)
    {
        SURGE_PROFILE_FUNC("Renderer2DPass::Shutdown");
        mRHI->DestroyDescriptorSet(mFrameDescriptorSet);
        mRHI->DestroyPipeline(m2DLinePipeline);
        mRHI->DestroyPipeline(m2DQuadPipeline);
        mRHI->DestroyPipeline(m2DTextPipeline);

        for(Uint i = 0; i < MAX_QUAD_BATCHES_PER_FRAME; i++)
            mRHI->DestroyDescriptorSet(mTexDescriptorSets[i]);

        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
            mRHI->DestroyBuffer(mQuadVB[i]);

        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
            mRHI->DestroyBuffer(mLineVB[i]);

        mRHI->DestroyBuffer(mQuadIB);
    }

    void Renderer2DPass::RenderStatsViaImGui(FrameBlackboard&)
    {
        // Vertex buffer fill: green, orange past 65%, red past 95%
        const auto usageBar = [](Uint used, Uint capacity, size_t vertexSize) {
            const float ratio = static_cast<float>(used) / static_cast<float>(capacity);
            const ImVec4 color = ratio > 0.95f ? ImVec4(1.0f, 0.0f, 0.0f, 1.0f) : ratio > 0.65f ? ImVec4(1.0f, 0.4f, 0.0f, 1.0f) : ImVec4(0.0f, 0.7f, 0.0f, 1.0f);
            char overlay[64];
            *std::format_to_n(overlay, sizeof(overlay) - 1, "{} / {} vertices ({:.1f} MB)", used, capacity, static_cast<double>(vertexSize) * used / (1024.0 * 1024.0)).out = '\0';
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
            ImGui::ProgressBar(ratio, ImVec2(-1.0f, 0.0f), overlay);
            ImGui::PopStyleColor();
        };

        ImGui::SeparatorText("Renderer2D");
        ImGui::Text("Quads: %u  |  Batches: %u / %u", mTotalQuadlVertexCount / 4, mQuadBatchCount, MAX_QUAD_BATCHES_PER_FRAME);
        usageBar(mTotalQuadlVertexCount, MAX_QUAD_VERTICES, sizeof(QuadVertex));
        if(mMaxQuadCountReached)
            ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.25f, 1.0f), "Quad limit reached, some quads were not drawn");

        ImGui::Text("Lines: %u  |  Batches: %u / %u", mTotalLineCount, (mTotalLineCount + MAX_LINES_PER_BATCH - 1) / MAX_LINES_PER_BATCH, (MAX_LINES_TOTAL + MAX_LINES_PER_BATCH - 1) / MAX_LINES_PER_BATCH);
        usageBar(mTotalLineVertexCount, MAX_LINE_VERTICES, sizeof(LineVertex));
        if(mMaxLinesCountReached)
            ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.25f, 1.0f), "Line limit reached, some lines were not drawn");
    }
}

