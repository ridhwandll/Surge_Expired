// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Graphics/RHI/RHIFrameContext.hpp"
#include "Surge/Graphics/RenderGraph/FrameBlackboard.hpp"

namespace Surge
{
    enum class PassGroup : uint8_t
    {
        SHADOW,
        MAIN_SCENE,
        OUTLINE_MASK,
        POST_PROCESS,
        UI_OVERLAY,
        SWAPCHAIN
    };

    class GraphicsRHI;
    class RenderPass
    {
    public:
        RenderPass() = default;
        virtual ~RenderPass() = default;
        SURGE_DISABLE_COPY_AND_MOVE(RenderPass);

        virtual void Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard) = 0;
        virtual void Execute(const FrameContext& ctx, const FrameBlackboard& blackboard) = 0;
        virtual void Resize(Uint width, Uint height, FrameBlackboard& blackBoard) = 0;
        virtual void OnImGuiRender(FrameBlackboard& blackBoard) = 0;
        virtual void Shutdown(FrameBlackboard& blackBoard) = 0;

        const String& GetName() const { return mName; }
        PassGroup GetGroup() const { return mGroup; }

        void SetEnabled(bool isEnabled) { mEnabled = isEnabled; }
        bool IsEnabled() { return mEnabled; }
        const Vector<ImageHandle>& GetImageReads() const { return mImageReads; }
        const Vector<ImageHandle>& GetImageWrites() const { return mImageWrites; }
    protected:
        
    protected:
        // Each pass sets this in constructor
        String mName;
        PassGroup mGroup;

        bool mEnabled = true;

        Vector<ImageHandle> mImageReads;
        Vector<ImageHandle> mImageWrites;
    };
}
