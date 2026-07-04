// Copyright (c) - SurgeTechnologies - All rights reserved
#include "UIOverlayPass.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Core/Profiler.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include <glm/gtc/matrix_transform.hpp>

namespace Surge
{
    static constexpr Uint MAX_UI_QUAD_VERTICES = UIOverlayPass::MAX_UI_QUADS_TOTAL * 4;
    static constexpr Uint MAX_UI_QUAD_INDICES = UIOverlayPass::MAX_UI_QUADS_PER_BATCH * 6;

    void UIOverlayPass::Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard)
    {
        SURGE_PROFILE_FUNC("UIOverlayPass::Setup");
        mRHI = rhi;

        glm::uvec2 size = Core::GetWindow()->GetSize();

        //auto ctx =  mRHI->GetBackendRHI().BeginOneTimeCommands();
        //mRHI->GetBackendRHI().CmdTransitionImageLayout(ctx, blackBoard.FinalImage, ImageUsage::COLOR_ATTACHMENT);
        //mRHI->GetBackendRHI().EndOneTimeCommands(ctx);

        FramebufferAttachment colorAttachment = {};
        colorAttachment.Handle = blackBoard.FinalImage;
        colorAttachment.Load = LoadOp::LOAD;
        colorAttachment.Store = StoreOp::STORE;

        FramebufferDesc fbDesc = {};
        fbDesc.ColorAttachments[0] = colorAttachment;
        fbDesc.ColorAttachmentCount = 1;
        fbDesc.HasDepth = false;
        fbDesc.Width = size.x;
        fbDesc.Height = size.y;
        fbDesc.DebugName = "UIOverlayFramebuffer";
        blackBoard.UIOverlayFramebuffer = mRHI->CreateFramebuffer(fbDesc);

        // 1. Setup VB and IB for UI Quads
        Vector<Uint> indices(MAX_UI_QUAD_INDICES);
        Uint offset = 0;
        for(Uint i = 0; i < MAX_UI_QUAD_INDICES; i += 6)
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
        ibDesc.Size = sizeof(Uint) * MAX_UI_QUAD_INDICES;
        ibDesc.Usage = BufferUsage::INDEX;
        ibDesc.HostVisible = false;
        ibDesc.InitialData = indices.data();
        ibDesc.DebugName = "UIBatchIB";
        mQuadIB = mRHI->CreateBuffer(ibDesc);

        BufferDesc vbDesc = {};
        vbDesc.Size = sizeof(Renderer2DPass::QuadVertex) * MAX_UI_QUAD_VERTICES;
        vbDesc.Usage = BufferUsage::VERTEX;
        vbDesc.HostVisible = true;
        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
        {
            vbDesc.DebugName = std::format("UIBatchVB Frame: {}", i);
            mQuadVB[i] = mRHI->CreateBuffer(vbDesc);
        }

        mCurrentQuadBatch.Reset();
        mCurrentQuadBatch.VertexData.resize(MAX_UI_QUADS_PER_BATCH * 4);

        // Frame UBO (Orthographic Matrix)
        BufferDesc uboDesc = {};
        uboDesc.Size = sizeof(FrameUBO);
        uboDesc.Usage = BufferUsage::UNIFORM;
        uboDesc.HostVisible = true;
        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
        {
            uboDesc.DebugName = std::format("UIFrameUBO Frame: {}", i);
            mUIFrameUBOs[i] = mRHI->CreateBuffer(uboDesc);
        }

        // UI Quad Pipeline (Depth testing explicitly DISABLED)
        PipelineDesc pd = {};
        pd.Shader_ = Core::GetRenderer()->GetShaderManager().Get("UIQuad.glsl");
        pd.DebugName = "UIOverlay_Quad";
        pd.Raster.Topo = Topology::TRIANGLE_LIST;
        pd.Raster.Polygon = PolygonMode::FILL;
        pd.Raster.Cull = CullMode::NONE;
        pd.Blend.Enable = true;
        pd.Blend.DstAlpha = BlendFactor::ONE_MINUS_SRC_ALPHA;
        pd.Depth.TestEnable = false;
        pd.Depth.WriteEnable = false;
        pd.TargetFramebuffer = blackBoard.UIOverlayFramebuffer;
        pd.TargetSwapchain = false;
        mUIQuadPipeline = mRHI->CreatePipeline(pd);

        // Text Pipeline
        PipelineDesc textPd = pd;
        textPd.Shader_ = Core::GetRenderer()->GetShaderManager().Get("UIText.glsl");
        textPd.DebugName = "UIOverlay_Text";
        mUITextPipeline = mRHI->CreatePipeline(textPd);

        // Descriptor Sets
        mUIFrameDescriptorSet = mRHI->CreateDescriptorSet(mUIQuadPipeline, DescriptorSetSlot::ZERO, DescriptorUpdateFrequency::DYNAMIC, "UIOverlayFrame [Set0]");
        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
        {
            DescriptorWrite write = {};
            write.Binding = 0;
            write.Type = DescriptorType::UNIFORM_BUFFER;
            write.Buffer = mUIFrameUBOs[i];
            write.BufferOffset = 0;
            write.BufferRange = sizeof(FrameUBO);
            mRHI->UpdateDescriptorSet(mUIFrameDescriptorSet, &write, 1, i);
        }

        mTexDescriptorSets.resize(MAX_UI_QUAD_BATCHES);
        for(Uint i = 0; i < MAX_UI_QUAD_BATCHES; i++)
        {
            mTexDescriptorSets[i] = mRHI->CreateDescriptorSet(mUIQuadPipeline, DescriptorSetSlot::ONE, DescriptorUpdateFrequency::DYNAMIC, std::format("UIOverlay_TexSet_{}", i).c_str());
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

        mImageReads.push_back(blackBoard.FinalImage);
        mImageWrites.push_back(blackBoard.FinalImage);
    }

    void UIOverlayPass::Execute(const FrameContext& ctx, const FrameBlackboard& blackboard)
    {
        SURGE_PROFILE_FUNC("UIOverlayPass::Execute");
        mCurrentFrameCtx = ctx;

        if(blackboard.UISpriteList.empty() && blackboard.UITextList.empty())
            return;

        //Log<Severity::Info>("UIOverlay Metrics: {} sprites, {} texts", blackboard.UISpriteList.size(), blackboard.UITextList.size());

        // Orthographic Matrix
        glm::vec2 size = { blackboard.ScreenWidth, blackboard.ScreenHeight };
        struct UIFrameUBO
        {
            glm::mat4 ViewProjection;
        } uiUboData;
        uiUboData.ViewProjection = glm::ortho(0.0f, size.x, 0.0f, size.y, -1.0f, 1.0f);
        mRHI->UploadBuffer(mUIFrameUBOs[ctx.FrameIndex], &uiUboData, sizeof(FrameUBO), 0);

        mQuadBatchCount = 0;
        mTotalQuadVertexCount = 0;
        mTotalQuadCount = 0;
        mCurrentFrameVertexOffset = 0;
        mMaxQuadCountReached = false;

        // ==========================================================================
        // UI SPRITES
        // ==========================================================================
        for(const QuadSubmitCmd& quad : blackboard.UISpriteList)
        {
            if(mCurrentQuadBatch.QuadCount >= MAX_UI_QUADS_PER_BATCH) FlushQuadBatch();
            if(mQuadBatchCount >= MAX_UI_QUAD_BATCHES || mTotalQuadCount == MAX_UI_QUADS_TOTAL)
            {
                mMaxQuadCountReached = true; break;
            }

            int slot = mCurrentQuadBatch.FindOrAssignTextureSlot(quad.Texture);
            if(slot == Renderer2DPass::QuadBatchData::MAX_TEX_IN_BATCH_REACHED)
            {
                FlushQuadBatch();
                if(mQuadBatchCount >= MAX_UI_QUAD_BATCHES) break;
                slot = mCurrentQuadBatch.FindOrAssignTextureSlot(quad.Texture);
            }

            const Uint packedColor = glm::packUnorm4x8(quad.Color);

            // Inverted V-coordinates so Sprites render right-side up in Y-Down space!
            static constexpr glm::vec2 sUVs[4] = { { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f } };
            static constexpr glm::vec4 sLocalPositions[4] = { { 0.5f, -0.5f, 0.0f, 1.0f}, { 0.5f,  0.5f, 0.0f, 1.0f}, {-0.5f,  0.5f, 0.0f, 1.0f}, {-0.5f, -0.5f, 0.0f, 1.0f} };

            for(Uint i = 0; i < 4; i++)
            {
                Renderer2DPass::QuadVertex& v = mCurrentQuadBatch.VertexData[mCurrentQuadBatch.VertexCount++];
                v.Position = quad.Transform * sLocalPositions[i];
                v.Color = packedColor;
                v.UV = sUVs[i];
                v.TextureIndex = (Uint)slot;
            }
            mCurrentQuadBatch.QuadCount++;
            mTotalQuadCount++;
        }

        if(mCurrentQuadBatch.QuadCount > 0) FlushQuadBatch();

        if(mQuadBatchCount > 0)
        {
            mRHI->CmdBindPipeline(ctx, mUIQuadPipeline);
            mRHI->CmdBindDescriptorSet(ctx, mUIQuadPipeline, mUIFrameDescriptorSet, DescriptorSetSlot::ZERO);
            mRHI->CmdBindVertexBuffer(ctx, mQuadVB[ctx.FrameIndex], 0);
            mRHI->CmdBindIndexBuffer(ctx, mQuadIB, 0);

            for(Uint i = 0; i < mQuadBatchCount; i++)
            {
                const Renderer2DPass::QuadDrawCmd& cmd = mQuadDrawCommands[i];
                mRHI->CmdBindDescriptorSet(ctx, mUIQuadPipeline, mTexDescriptorSets[i], DescriptorSetSlot::ONE);
                mRHI->CmdDrawIndexed(ctx, cmd.QuadCount * 6, 1, 0, (int32_t)cmd.VertexOffset, 0);
            }
        }

        // ==========================================================================
        // UI TEXT
        // ==========================================================================
        Uint textBatchStartIndex = mQuadBatchCount;

        struct TextPushConstants { float PxRange; float pad[2]; };
        std::array<TextPushConstants, MAX_UI_QUAD_BATCHES> textBatchParams;
        TextPushConstants currentTextParams = {};

        for(const TextSubmitCmd& txt : blackboard.UITextList)
        {
            if(!txt.FontAsset || txt.Text.empty() || mMaxQuadCountReached) continue;

            glm::mat4 activeTransform = txt.Transform;
            ImageHandle fontAtlas = txt.FontAsset->GetAtlas();
            const Uint packedColorText = glm::packUnorm4x8(txt.Color);
            //const Uint packedColorShadow = glm::packUnorm4x8(txt.ShadowColor);

            glm::vec4 localPositions[4];
            glm::vec2 uvs[4];

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
                        currentLineWidth = 0.0f; measurePrevChar = 0; continue;
                    }
                }
                if(c == '\n') { mLineLayoutCache.push_back(currentLineWidth); currentLineWidth = 0.0f; measurePrevChar = 0; continue; }

                const FontGlyph* glyph = txt.FontAsset->GetGlyph(c);
                if(!glyph) continue;

                if(measurePrevChar != 0) currentLineWidth += txt.FontAsset->GetKerning(measurePrevChar, c);
                currentLineWidth += (glyph->Advance + txt.LetterSpacing);
                measurePrevChar = c;
            }
            mLineLayoutCache.push_back(currentLineWidth);

            auto getCursorX = [&](Uint lineIdx) -> float {
                if(lineIdx >= mLineLayoutCache.size()) [[unlikely]] return 0.0f;
                float lineWidth = mLineLayoutCache[lineIdx];
                if(txt.Alignment == TextAlignment::CENTER) return -lineWidth * 0.5f;
                else if(txt.Alignment == TextAlignment::RIGHT) return -lineWidth;
                return 0.0f;
            };

            auto buildTextQuads = [&](glm::vec2 posOffset, float zOffset, Uint packedColor) {
                currentTextParams.PxRange = txt.FontAsset->GetPxRange();
                textBatchParams[mQuadBatchCount] = currentTextParams;
                Uint lineIndex = 0;
                Uint drawPrevChar = 0;
                float italicSkew = txt.Italic ? 0.25f : 0.0f;

                // (Rid) In MSDF EM units, visual letters are typically ~70% of the line height (capHeight)
                // The descending tails (p, g, y) typically extend ~20% below the baseline (descent)
                float capHeight = txt.FontAsset->GetLineHeight() * 0.7f;
                float descent = txt.FontAsset->GetLineHeight() * 0.2f;
                Uint numLines = (Uint)mLineLayoutCache.size();
                float blockDrop = numLines > 0 ? ((numLines - 1) * (txt.FontAsset->GetLineHeight() + txt.LineSpacing)) : 0.0f;

                float cursorY = 0.0f; // Default BASELINE
                if(txt.VerticalAlignment == TextVerticalAlignment::CENTER)
                    cursorY = (capHeight - descent - blockDrop) * 0.5f;
                else if(txt.VerticalAlignment == TextVerticalAlignment::TOP)
                    cursorY = capHeight;
                else if(txt.VerticalAlignment == TextVerticalAlignment::BOTTOM)
                    cursorY = -blockDrop - descent;

                float cursorX = getCursorX(lineIndex);

                auto nextLine = [&]() {
                    // Since +Y goes DOWN the screen, next line must INCREASE the cursorY
                    cursorY += (txt.FontAsset->GetLineHeight() + txt.LineSpacing);
                    lineIndex++;
                    cursorX = getCursorX(lineIndex);
                    drawPrevChar = 0;
                };

                for(size_t i = 0; i < txt.Text.size(); i++)
                {
                    if(mTotalQuadCount >= MAX_UI_QUADS_TOTAL) { mMaxQuadCountReached = true; break; }
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
                        if(baseWidth + nextWordLength > txt.MaxWidth) { nextLine(); continue; }
                    }

                    if(c == '\n') { nextLine(); continue; }

                    const FontGlyph* glyph = txt.FontAsset->GetGlyph(c);
                    if(!glyph) continue;

                    if(drawPrevChar != 0) cursorX += txt.FontAsset->GetKerning(drawPrevChar, c);

                    int slot = mCurrentQuadBatch.FindOrAssignTextureSlot(fontAtlas);
                    if(slot == Renderer2DPass::QuadBatchData::MAX_TEX_IN_BATCH_REACHED || mCurrentQuadBatch.QuadCount >= MAX_UI_QUADS_PER_BATCH)
                    {
                        textBatchParams[mQuadBatchCount] = currentTextParams;
                        FlushQuadBatch();
                        if(mTotalQuadCount == MAX_UI_QUADS_TOTAL || mQuadBatchCount >= MAX_UI_QUAD_BATCHES) { mMaxQuadCountReached = true; break; }
                        slot = mCurrentQuadBatch.FindOrAssignTextureSlot(fontAtlas);
                        textBatchParams[mQuadBatchCount] = currentTextParams;
                    }

                    // Invert MSDF PlaneBounds Y so Text renders right-side up
                    glm::vec2 quadMin = glm::vec2(cursorX + glyph->PlaneBounds[0].x + posOffset.x, cursorY - glyph->PlaneBounds[1].y + posOffset.y);
                    glm::vec2 quadMax = glm::vec2(cursorX + glyph->PlaneBounds[1].x + posOffset.x, cursorY - glyph->PlaneBounds[0].y + posOffset.y);

                    localPositions[0] = { quadMax.x + (glyph->PlaneBounds[1].y * italicSkew), quadMin.y, zOffset, 1.0f };
                    localPositions[1] = { quadMax.x + (glyph->PlaneBounds[0].y * italicSkew), quadMax.y, zOffset, 1.0f };
                    localPositions[2] = { quadMin.x + (glyph->PlaneBounds[0].y * italicSkew), quadMax.y, zOffset, 1.0f };
                    localPositions[3] = { quadMin.x + (glyph->PlaneBounds[1].y * italicSkew), quadMin.y, zOffset, 1.0f };

                    uvs[0] = { glyph->UVBounds[1].x, 1.0f - glyph->UVBounds[1].y };
                    uvs[1] = { glyph->UVBounds[1].x, 1.0f - glyph->UVBounds[0].y };
                    uvs[2] = { glyph->UVBounds[0].x, 1.0f - glyph->UVBounds[0].y };
                    uvs[3] = { glyph->UVBounds[0].x, 1.0f - glyph->UVBounds[1].y };

                    for(Uint vIdx = 0; vIdx < 4; vIdx++)
                    {
                        Renderer2DPass::QuadVertex& v = mCurrentQuadBatch.VertexData[mCurrentQuadBatch.VertexCount++];
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
            };

            // NO SHADOWS in UI for Now
            //if(txt.EnableShadow && txt.ShadowColor.a > 0.001f)
            //    buildTextQuads(txt.ShadowOffset, -0.01f, packedColorShadow);

            buildTextQuads({ 0.0f, 0.0f }, 0.0f, packedColorText);
        }

        if(mCurrentQuadBatch.QuadCount > 0)
        {
            textBatchParams[mQuadBatchCount] = currentTextParams;
            FlushQuadBatch();
        }

        if(mQuadBatchCount > textBatchStartIndex)
        {
            mRHI->CmdBindPipeline(ctx, mUITextPipeline);
            mRHI->CmdBindDescriptorSet(ctx, mUITextPipeline, mUIFrameDescriptorSet, DescriptorSetSlot::ZERO);
            mRHI->CmdBindVertexBuffer(ctx, mQuadVB[ctx.FrameIndex], 0);
            mRHI->CmdBindIndexBuffer(ctx, mQuadIB, 0);

            for(Uint i = textBatchStartIndex; i < mQuadBatchCount; i++)
            {
                const TextPushConstants& params = textBatchParams[i];
                mRHI->CmdPushConstants(ctx, mUITextPipeline, ShaderType::VERTEX | ShaderType::FRAGMENT, 0, sizeof(TextPushConstants), &params);

                const Renderer2DPass::QuadDrawCmd& cmd = mQuadDrawCommands[i];
                mRHI->CmdBindDescriptorSet(ctx, mUITextPipeline, mTexDescriptorSets[i], DescriptorSetSlot::ONE);
                mRHI->CmdDrawIndexed(ctx, cmd.QuadCount * 6, 1, 0, (int32_t)cmd.VertexOffset, 0);
            }
        }
    }

    void UIOverlayPass::FlushQuadBatch()
    {
        if(mCurrentQuadBatch.QuadCount == 0) return;

        if(mQuadBatchCount >= MAX_UI_QUAD_BATCHES)
        {
            Log<Severity::Error>("UIOverlayPass: Exceeded max Quad batches!");
            return;
        }

        const FrameContext& ctx = mCurrentFrameCtx;
        const Uint batchIndex = mQuadBatchCount;

        for(Uint slot = 0; slot < mCurrentQuadBatch.TextureCount; slot++)
        {
            if(mCurrentQuadBatch.Textures[slot].IsNull())
                continue;

            DescriptorWrite write = {};
            write.Binding = 0;
            write.Type = DescriptorType::TEXTURE;
            write.ArrayIndex = slot;
            write.Texture = mCurrentQuadBatch.Textures[slot];
            write.Sampler = Core::GetRenderer()->GetTextSampler();
            mRHI->UpdateDescriptorSet(mTexDescriptorSets[batchIndex], &write, 1, ctx.FrameIndex);
        }

        const Uint uploadOffsetInBytes = mCurrentFrameVertexOffset * sizeof(Renderer2DPass::QuadVertex);
        const Uint uploadSizeInBytes = mCurrentQuadBatch.VertexCount * sizeof(Renderer2DPass::QuadVertex);
        mRHI->UploadBuffer(mQuadVB[ctx.FrameIndex], mCurrentQuadBatch.VertexData.data(), uploadSizeInBytes, uploadOffsetInBytes);

        mQuadDrawCommands[mQuadBatchCount++] = { mCurrentFrameVertexOffset, mCurrentQuadBatch.QuadCount };

        mTotalQuadVertexCount += mCurrentQuadBatch.VertexCount;
        mCurrentFrameVertexOffset += mCurrentQuadBatch.VertexCount;
        mCurrentQuadBatch.Reset();
    }

    void UIOverlayPass::Resize(Uint width, Uint height, FrameBlackboard& blackBoard)
    {
        constexpr bool resizeFinalImage = false; // We do not own the FinalImage, so we DO NOT Resize it
        Core::AddFrameEndCallback([this, width, height, &blackBoard]() { mRHI->ResizeFramebuffer(blackBoard.UIOverlayFramebuffer, width, height, resizeFinalImage); });
    }

    void UIOverlayPass::OnImGuiRender(FrameBlackboard&) {}

    void UIOverlayPass::Shutdown(FrameBlackboard& blackBoard)
    {
        mRHI->DestroyPipeline(mUIQuadPipeline);
        mRHI->DestroyPipeline(mUITextPipeline);
        mRHI->DestroyDescriptorSet(mUIFrameDescriptorSet);

        for(Uint i = 0; i < MAX_UI_QUAD_BATCHES; i++)
            mRHI->DestroyDescriptorSet(mTexDescriptorSets[i]);

        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
        {
            mRHI->DestroyBuffer(mQuadVB[i]);
            mRHI->DestroyBuffer(mUIFrameUBOs[i]);
        }

        mRHI->DestroyBuffer(mQuadIB);
        mRHI->DestroyFramebuffer(blackBoard.UIOverlayFramebuffer);
    }
}