// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RHI/RHIFrameContext.hpp"
#include "FrameBlackboard.hpp"

namespace Surge
{
    class GraphicsRHI;
    class RenderPass
    {
    public:
        virtual ~RenderPass() = default;

        virtual void Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard) = 0;
        virtual void Execute(const FrameContext& ctx, const FrameBlackboard& blackboard) = 0;
        virtual void Resize(Uint width, Uint height, FrameBlackboard& blackBoard) = 0;
        virtual void OnImGuiRender(FrameBlackboard& blackBoard) = 0;
        virtual void Shutdown() = 0;

        const String& GetName()  const { return mName; }
    protected:
        String mName;
    };
}
