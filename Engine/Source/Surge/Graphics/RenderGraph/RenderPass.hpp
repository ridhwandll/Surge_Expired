// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Graphics/RHI/RHIFrameContext.hpp"
#include "Surge/Graphics/RenderGraph/FrameBlackboard.hpp"
#include <glm/glm.hpp>
#include <functional>

namespace Surge
{
    enum class PassGroup : uint8_t
    {
        SHADOW,
        MAIN_SCENE,
        AMBIENT_OCCLUSION,
        BLOOM,
        POST_PROCESS,
        UI_OVERLAY,
        SWAPCHAIN
    };

    class GraphicsRHI;
    class ShaderManager;

    // What a RenderGraph and its passes use from outside the graph, filled by the Renderer: passes never reach into Core
    struct RenderGraphContext
    {
        GraphicsRHI* RHI = nullptr;
        ShaderManager* Shaders = nullptr;
        SamplerHandle TextSampler;
        glm::vec2 WindowSize = { 1.0f, 1.0f }; // When the graph was set up, Resize() follows
        bool ImGuiEnabled = false;
        void (*DeferToFrameEnd)(const std::function<void()>& work) = nullptr; // Runs work at the very end of the frame (Core's AddFrameEndCallback)
    };

    class RenderPass
    {
    public:
        RenderPass() = default;
        virtual ~RenderPass() = default;
        SURGE_DISABLE_COPY_AND_MOVE(RenderPass);

        virtual void Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard) = 0;
        virtual void Execute(const FrameContext& ctx, const FrameBlackboard& blackboard) = 0;

        // Called for every enabled pass before any render pass of the frame begins: culling, instance/uniform uploads, Descriptor updates or framebuffer reconfiguration that must not happen inside a render pass
        virtual void Prepare([[maybe_unused]] const FrameContext& ctx, [[maybe_unused]] FrameBlackboard& blackboard) {}

        // Groups whose passes all report no work this frame are skipped entirely (no render pass begin/clear/store)
        virtual bool HasWork([[maybe_unused]] const FrameBlackboard& blackboard) const { return true; }
        virtual void Resize(Uint width, Uint height, FrameBlackboard& blackBoard) = 0;
        virtual void RenderStatsViaImGui(FrameBlackboard& blackBoard) = 0;
        virtual void Shutdown(FrameBlackboard& blackBoard) = 0;

        const String& GetName() const { return mName; }
        PassGroup GetGroup() const { return mGroup; }

        void SetEnabled(bool isEnabled) { mEnabled = isEnabled; }
        bool IsEnabled() { return mEnabled; }
        const Vector<ImageHandle>& GetImageReads() const { return mImageReads; }
        const Vector<ImageHandle>& GetImageWrites() const { return mImageWrites; }
    protected:
        
    protected:
        // Each pass sets these in constructor
        String mName;
        PassGroup mGroup;

        bool mEnabled = true;

        Vector<ImageHandle> mImageReads;
        Vector<ImageHandle> mImageWrites;

        RenderGraphContext mContext; // Set by the RenderGraph right before Setup

        friend class RenderGraph;
    };
}
