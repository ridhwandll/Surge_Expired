// Copyright (c) - SurgeTechnologies - All rights reserved
#include "UIOverlayPass.hpp"
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

        glm::uvec2 size = mContext.WindowSize;

        if(!RHISettings::RENDER_TO_SWAPCHAIN)
        {
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
        }

        // Setup VB and IB for UI Quads
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

        // UI Pipeline, sprites + MSDF text (Depth testing explicitly DISABLED)
        PipelineDesc pd = {};
        pd.Shader_ = mContext.Shaders->Get("UI.glsl");
        pd.DebugName = "UIOverlay";
        pd.Raster.Topo = Topology::TRIANGLE_LIST;
        pd.Raster.Polygon = PolygonMode::FILL;
        pd.Raster.Cull = CullMode::NONE;
        pd.Blend.Enable = true;
        pd.Blend.DstAlpha = BlendFactor::ONE_MINUS_SRC_ALPHA;
        pd.Depth.TestEnable = false;
        pd.Depth.WriteEnable = false;
        pd.TargetFramebuffer = RHISettings::RENDER_TO_SWAPCHAIN ? FramebufferHandle::Invalid() : blackBoard.UIOverlayFramebuffer;
        pd.TargetSwapchain = RHISettings::RENDER_TO_SWAPCHAIN;
        mUIPipeline = mRHI->CreatePipeline(pd);

        mWhiteImage = blackBoard.WhiteImage;
        mTexDescriptorSets.resize(MAX_UI_QUAD_BATCHES);
        for(Uint i = 0; i < MAX_UI_QUAD_BATCHES; i++)
        {
            mTexDescriptorSets[i] = mRHI->CreateDescriptorSet(mUIPipeline, DescriptorSetSlot::ZERO, DescriptorUpdateFrequency::DYNAMIC, std::format("UIOverlay_TexSet_{}", i).c_str());
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

        if(!RHISettings::RENDER_TO_SWAPCHAIN)
        {
            mImageReads.push_back(blackBoard.FinalImage);
            mImageWrites.push_back(blackBoard.FinalImage);
        }
    }

    void UIOverlayPass::Execute(const FrameContext& ctx, const FrameBlackboard& blackboard)
    {
        SURGE_PROFILE_FUNC("UIOverlayPass::Execute");
        mCurrentFrameCtx = ctx;

        if(blackboard.UIDrawOrder.empty())
            return;

        mQuadBatchCount = 0;
        mTotalQuadVertexCount = 0;
        mTotalQuadCount = 0;
        mCurrentFrameVertexOffset = 0;
        mMaxQuadCountReached = false;

        // Submission order == hierarchy order, sprites and text interleave correctly and still batch together
        for(const UIDrawItem& item : blackboard.UIDrawOrder)
        {
            if(mMaxQuadCountReached)
                break;

            const glm::vec4* clip = item.ClipIndex != UI_NO_CLIP ? &blackboard.UIClipRects[item.ClipIndex - 1] : nullptr;
            if(item.IsText)
                AppendText(blackboard.UITextList[item.Index], clip);
            else
                AppendSprite(blackboard.UISpriteList[item.Index], clip);
        }

        if(mCurrentQuadBatch.QuadCount > 0)
            FlushQuadBatch();

        if(mQuadBatchCount == 0)
            return;

        // Orthographic Matrix
        const glm::vec2 size = blackboard.UIResolution.x > 0.0f ? blackboard.UIResolution : glm::vec2(blackboard.ScreenWidth, blackboard.ScreenHeight);
        glm::mat4 viewProjection = glm::ortho(0.0f, size.x, 0.0f, size.y, -1.0f, 1.0f);
        if(const Uint rotation = RHISettings::RENDER_TO_SWAPCHAIN ? mRHI->GetSurfaceRotation() : 0; rotation != 0)
            viewProjection = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f * static_cast<float>(rotation)), glm::vec3(0.0f, 0.0f, 1.0f)) * viewProjection; // Pre-rotation

        mRHI->CmdBindPipeline(ctx, mUIPipeline);
        mRHI->CmdPushConstants(ctx, mUIPipeline, ShaderType::VERTEX | ShaderType::FRAGMENT, 0, sizeof(glm::mat4), &viewProjection);
        mRHI->CmdBindVertexBuffer(ctx, mQuadVB[ctx.FrameIndex], 0);
        mRHI->CmdBindIndexBuffer(ctx, mQuadIB, 0);

        for(Uint i = 0; i < mQuadBatchCount; i++)
        {
            const Renderer2DPass::QuadDrawCmd& cmd = mQuadDrawCommands[i];
            mRHI->CmdBindDescriptorSet(ctx, mUIPipeline, mTexDescriptorSets[i], DescriptorSetSlot::ZERO);
            mRHI->CmdDrawIndexed(ctx, cmd.QuadCount * 6, 1, 0, (int32_t)cmd.VertexOffset, 0);
        }
    }

    bool UIOverlayPass::PushQuad(glm::vec4 (&positions)[4], glm::vec2 (&uvs)[4], Uint packedColor, ImageHandle texture, Uint textureIndexFlags, const glm::vec4* clip)
    {
        if(clip)
        {
            // Axis aligned bounds (italic glyphs are skewed, they are clipped as their bounding box)
            const float left = glm::min(positions[2].x, positions[3].x);
            const float right = glm::max(positions[0].x, positions[1].x);
            const float top = positions[0].y;
            const float bottom = positions[1].y;
            if(right <= clip->x || left >= clip->z || bottom <= clip->y || top >= clip->w)
                return true; // Fully clipped, nothing to draw

            if(left < clip->x || right > clip->z || top < clip->y || bottom > clip->w)
            {
                // The UVs keep the same linear mapping, so UI.glsl still sees the original rect through the UV derivatives (rounded corners stay put)
                const float width = right - left;
                const float height = bottom - top;
                const float newLeft = glm::max(left, clip->x);
                const float newRight = glm::min(right, clip->z);
                const float newTop = glm::max(top, clip->y);
                const float newBottom = glm::min(bottom, clip->w);
                const float tL = width > 0.0f ? (newLeft - left) / width : 0.0f;
                const float tR = width > 0.0f ? (newRight - left) / width : 1.0f;
                const float tT = height > 0.0f ? (newTop - top) / height : 0.0f;
                const float tB = height > 0.0f ? (newBottom - top) / height : 1.0f;

                const glm::vec2 uvOrigin = uvs[3];        // Left top
                const glm::vec2 uvAlongX = uvs[0] - uvs[3]; // Left top -> right top
                const glm::vec2 uvAlongY = uvs[2] - uvs[3]; // Left top -> left bottom
                auto uvAt = [&](float tx, float ty) { return uvOrigin + uvAlongX * tx + uvAlongY * ty; };

                positions[0] = { newRight, newTop, positions[0].z, 1.0f };
                positions[1] = { newRight, newBottom, positions[1].z, 1.0f };
                positions[2] = { newLeft, newBottom, positions[2].z, 1.0f };
                positions[3] = { newLeft, newTop, positions[3].z, 1.0f };
                uvs[0] = uvAt(tR, tT);
                uvs[1] = uvAt(tR, tB);
                uvs[2] = uvAt(tL, tB);
                uvs[3] = uvAt(tL, tT);
            }
        }

        if(mTotalQuadCount >= MAX_UI_QUADS_TOTAL || mQuadBatchCount >= MAX_UI_QUAD_BATCHES)
        {
            mMaxQuadCountReached = true;
            return false;
        }

        if(mCurrentQuadBatch.QuadCount >= MAX_UI_QUADS_PER_BATCH)
            FlushQuadBatch();

        int slot = mCurrentQuadBatch.FindOrAssignTextureSlot(texture);
        if(slot == Renderer2DPass::QuadBatchData::MAX_TEX_IN_BATCH_REACHED)
        {
            FlushQuadBatch();
            slot = mCurrentQuadBatch.FindOrAssignTextureSlot(texture);
        }

        if(mQuadBatchCount >= MAX_UI_QUAD_BATCHES)
        {
            mMaxQuadCountReached = true;
            return false;
        }

        for(Uint i = 0; i < 4; i++)
        {
            Renderer2DPass::QuadVertex& v = mCurrentQuadBatch.VertexData[mCurrentQuadBatch.VertexCount++];
            v.Position = positions[i];
            v.Color = packedColor;
            v.UV = uvs[i];
            v.TextureIndex = static_cast<Uint>(slot) | textureIndexFlags;
        }
        mCurrentQuadBatch.QuadCount++;
        mTotalQuadCount++;
        return true;
    }

    void UIOverlayPass::AppendSprite(const UISpriteCmd& sprite, const glm::vec4* clip)
    {
        if(sprite.Max.x <= sprite.Min.x || sprite.Max.y <= sprite.Min.y || sprite.Color.a <= 0.0f)
            return;

        // UV (0,0) is the top left of the texture, y grows down like the UI
        glm::vec4 positions[4] = { { sprite.Max.x, sprite.Min.y, 0.0f, 1.0f }, { sprite.Max.x, sprite.Max.y, 0.0f, 1.0f }, { sprite.Min.x, sprite.Max.y, 0.0f, 1.0f }, { sprite.Min.x, sprite.Min.y, 0.0f, 1.0f } };
        glm::vec2 uvs[4] = { { sprite.UVMax.x, sprite.UVMin.y }, sprite.UVMax, { sprite.UVMin.x, sprite.UVMax.y }, sprite.UVMin };

        Uint flags = 0u;
        if(sprite.CornerRadius > 0.0f)
            flags = static_cast<Uint>(glm::clamp(sprite.CornerRadius * CORNER_RADIUS_SCALE, 1.0f, 65535.0f)) << CORNER_RADIUS_SHIFT;

        PushQuad(positions, uvs, glm::packUnorm4x8(sprite.Color), sprite.Texture, flags, clip);
    }

    void UIOverlayPass::AppendText(const TextSubmitCmd& txt, const glm::vec4* clip)
    {
        if(!txt.FontAsset || txt.Text.empty())
            return;

        const Font* font = txt.FontAsset;
        const ImageHandle fontAtlas = font->GetAtlas();
        const Uint packedColor = glm::packUnorm4x8(txt.Color);

        // MSDF px range travels with every glyph vertex, so different fonts can share a batch
        const Uint pxRangeFixed = static_cast<Uint>(glm::clamp(font->GetPxRange() * 256.0f, 0.0f, 65535.0f));
        const Uint glyphFlags = TEXT_GLYPH_FLAG | (pxRangeFixed << PX_RANGE_SHIFT);

        // Measure line widths (for alignment and word wrap)
        mLineLayoutCache.clear();
        float currentLineWidth = 0.0f;
        Uint measurePrevChar = 0;
        for(size_t i = 0; i < txt.Text.size(); i++)
        {
            const char c = txt.Text[i];
            if(txt.MaxWidth > 0.0f && c == ' ')
            {
                float nextWordLength = 0.0f;
                for(size_t j = i + 1; j < txt.Text.size() && txt.Text[j] != ' ' && txt.Text[j] != '\n'; j++)
                {
                    const FontGlyph* nextGlyph = font->GetGlyph(txt.Text[j]);
                    if(nextGlyph) nextWordLength += nextGlyph->Advance;
                }
                if(currentLineWidth + nextWordLength > txt.MaxWidth)
                {
                    mLineLayoutCache.push_back(currentLineWidth);
                    currentLineWidth = 0.0f; measurePrevChar = 0; continue;
                }
            }
            if(c == '\n') { mLineLayoutCache.push_back(currentLineWidth); currentLineWidth = 0.0f; measurePrevChar = 0; continue; }

            const FontGlyph* glyph = font->GetGlyph(c);
            if(!glyph) continue;

            if(measurePrevChar != 0) currentLineWidth += font->GetKerning(measurePrevChar, c);
            currentLineWidth += (glyph->Advance + txt.LetterSpacing);
            measurePrevChar = c;
        }
        mLineLayoutCache.push_back(currentLineWidth);

        auto getCursorX = [&](Uint lineIdx) -> float {
            if(lineIdx >= mLineLayoutCache.size()) [[unlikely]] return 0.0f;
            const float lineWidth = mLineLayoutCache[lineIdx];
            if(txt.Alignment == TextAlignment::CENTER) return -lineWidth * 0.5f;
            else if(txt.Alignment == TextAlignment::RIGHT) return -lineWidth;
            return 0.0f;
        };

        Uint lineIndex = 0;
        Uint drawPrevChar = 0;
        const float italicSkew = txt.Italic ? 0.25f : 0.0f;

        // (Rid) In MSDF EM units, visual letters are typically ~70% of the line height (capHeight)
        // The descending tails (p, g, y) typically extend ~20% below the baseline (descent)
        const float capHeight = font->GetLineHeight() * 0.7f;
        const float descent = font->GetLineHeight() * 0.2f;
        const Uint numLines = (Uint)mLineLayoutCache.size();
        const float blockDrop = numLines > 0 ? ((numLines - 1) * (font->GetLineHeight() + txt.LineSpacing)) : 0.0f;

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
            cursorY += (font->GetLineHeight() + txt.LineSpacing);
            lineIndex++;
            cursorX = getCursorX(lineIndex);
            drawPrevChar = 0;
        };

        glm::vec4 localPositions[4];
        glm::vec4 positions[4];
        glm::vec2 uvs[4];
        for(size_t i = 0; i < txt.Text.size(); i++)
        {
            const char c = txt.Text[i];
            if(txt.MaxWidth > 0.0f && c == ' ')
            {
                float nextWordLength = 0.0f;
                for(size_t j = i + 1; j < txt.Text.size() && txt.Text[j] != ' ' && txt.Text[j] != '\n'; j++)
                {
                    const FontGlyph* nextGlyph = font->GetGlyph(txt.Text[j]);
                    if(nextGlyph) nextWordLength += nextGlyph->Advance;
                }
                const float lineWidth = lineIndex < mLineLayoutCache.size() ? mLineLayoutCache[lineIndex] : 0.0f;
                const float baseWidth = txt.Alignment == TextAlignment::LEFT ? cursorX : cursorX + (txt.Alignment == TextAlignment::CENTER ? lineWidth * 0.5f : lineWidth);
                if(baseWidth + nextWordLength > txt.MaxWidth) { nextLine(); continue; }
            }

            if(c == '\n') { nextLine(); continue; }

            const FontGlyph* glyph = font->GetGlyph(c);
            if(!glyph) continue;

            if(drawPrevChar != 0) cursorX += font->GetKerning(drawPrevChar, c);

            // Invert MSDF PlaneBounds Y so Text renders right-side up
            const glm::vec2 quadMin = glm::vec2(cursorX + glyph->PlaneBounds[0].x, cursorY - glyph->PlaneBounds[1].y);
            const glm::vec2 quadMax = glm::vec2(cursorX + glyph->PlaneBounds[1].x, cursorY - glyph->PlaneBounds[0].y);

            localPositions[0] = { quadMax.x + (glyph->PlaneBounds[1].y * italicSkew), quadMin.y, 0.0f, 1.0f };
            localPositions[1] = { quadMax.x + (glyph->PlaneBounds[0].y * italicSkew), quadMax.y, 0.0f, 1.0f };
            localPositions[2] = { quadMin.x + (glyph->PlaneBounds[0].y * italicSkew), quadMax.y, 0.0f, 1.0f };
            localPositions[3] = { quadMin.x + (glyph->PlaneBounds[1].y * italicSkew), quadMin.y, 0.0f, 1.0f };

            uvs[0] = { glyph->UVBounds[1].x, 1.0f - glyph->UVBounds[1].y };
            uvs[1] = { glyph->UVBounds[1].x, 1.0f - glyph->UVBounds[0].y };
            uvs[2] = { glyph->UVBounds[0].x, 1.0f - glyph->UVBounds[0].y };
            uvs[3] = { glyph->UVBounds[0].x, 1.0f - glyph->UVBounds[1].y };

            for(Uint v = 0; v < 4; v++)
                positions[v] = txt.Transform * localPositions[v];

            if(!PushQuad(positions, uvs, packedColor, fontAtlas, glyphFlags, clip))
                return;

            cursorX += (glyph->Advance + txt.LetterSpacing);
            drawPrevChar = c;
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

        // Write every slot (unused ones get the white texture): these sets are reused across frames, a stale slot could still reference
        // a texture that was destroyed since, and the whole array is statically used by the shader, so it must stay valid
        std::array<DescriptorWrite, MAX_TEX_SLOTS_PER_BATCH - 1> writes = {};
        const SamplerHandle sampler = mContext.TextSampler;
        for(Uint slot = 1; slot < MAX_TEX_SLOTS_PER_BATCH; slot++) // Slot 0 is the white texture, written once in Setup
        {
            const bool used = slot < mCurrentQuadBatch.TextureCount && !mCurrentQuadBatch.Textures[slot].IsNull();

            DescriptorWrite& write = writes[slot - 1];
            write.Binding = 0;
            write.Type = DescriptorType::TEXTURE;
            write.ArrayIndex = slot;
            write.Texture = used ? mCurrentQuadBatch.Textures[slot] : mWhiteImage;
            write.Sampler = sampler;
        }
        mRHI->UpdateDescriptorSet(mTexDescriptorSets[batchIndex], writes.data(), static_cast<Uint>(writes.size()), ctx.FrameIndex);

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
        if(RHISettings::RENDER_TO_SWAPCHAIN)
            return;

        constexpr bool resizeFinalImage = false; // We do not own the FinalImage, so we DO NOT Resize it
        const FramebufferHandle framebuffer = blackBoard.UIOverlayFramebuffer;
        mContext.DeferToFrameEnd([this, width, height, framebuffer]() { mRHI->ResizeFramebuffer(framebuffer, width, height, resizeFinalImage); });
    }

    void UIOverlayPass::RenderStatsViaImGui(FrameBlackboard&) {}

    void UIOverlayPass::Shutdown(FrameBlackboard& blackBoard)
    {
        mRHI->DestroyPipeline(mUIPipeline);

        for(Uint i = 0; i < MAX_UI_QUAD_BATCHES; i++)
            mRHI->DestroyDescriptorSet(mTexDescriptorSets[i]);

        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
            mRHI->DestroyBuffer(mQuadVB[i]);

        mRHI->DestroyBuffer(mQuadIB);
        if(!blackBoard.UIOverlayFramebuffer.IsNull())
            mRHI->DestroyFramebuffer(blackBoard.UIOverlayFramebuffer);
    }
}