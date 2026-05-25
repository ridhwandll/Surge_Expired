// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RHI/RHIFrameContext.hpp"
#include "FrameBlackboard.hpp"

namespace Surge
{
    enum class PassGroup : uint8_t
    {
        //SHADOW,
        MAIN_SCENE,
        POST_PROCESS,
        SWAPCHAIN
    };

    class GraphicsRHI;
    class RenderPass
    {
    public:
        virtual ~RenderPass() = default;

        virtual void Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard) = 0;
        virtual void Execute(const FrameContext& ctx, const FrameBlackboard& blackboard) = 0;
        virtual void Resize(Uint width, Uint height, FrameBlackboard& blackBoard) = 0;
        virtual void OnImGuiRender(FrameBlackboard& blackBoard) = 0;
        virtual void Shutdown(FrameBlackboard& blackBoard) = 0;

        PassGroup GetGroup() const { return mGroup; }
        const String& GetName() const { return mName; }
        void SetEnabled(bool isEnabled) { mEnabled = isEnabled; }
        bool IsEnabled() { return mEnabled; }
        const Vector<ImageHandle>& GetImageReads() const { return mImageReads; }
        const Vector<ImageHandle>& GetImageWrites() const { return mImageWrites; }
    protected:
        
    protected:
        PassGroup mGroup = PassGroup::MAIN_SCENE; // each pass sets this in constructor
        String mName;

        bool mEnabled = true;
        Vector<ImageHandle> mImageReads;
        Vector<ImageHandle> mImageWrites;
    };
}
