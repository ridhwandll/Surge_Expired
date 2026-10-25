// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Panels/IPanel.hpp"
#include "Panels/RenderGraphTarget.hpp"
#include "Panels/RenderGraphVisualizer.hpp"

namespace Surge
{
    struct FrameTimings;
    struct FrameHistory;

    // One window for everything rendering:
    //  - RHI:      GPU, memory and RHI pool metrics (render settings live in the scene's RendererSettingsComponent)
    //  - Graph:    the selected graph's groups/passes/barriers + a pass inspector (RenderGraphVisualizer)
    //  - Stats:    the selected graph's per-pass stats (RenderPass::OnImGuiRender)
    //  - Performance: CPU work and GPU timings of the whole frame (every view's graph, then the main graph), from Core's Diagnostics
    class RendererPanel : public IPanel
    {
    public:
        RendererPanel() = default;
        virtual ~RendererPanel() override = default;

        virtual void Init(void* panelInitArgs) override;
        virtual void OnEvent([[maybe_unused]] Event& e) override {}
        virtual void Render(bool* show) override;
        virtual void Shutdown() override {}

        static PanelCode GetStaticCode() { return PanelCode::Renderer; }

    private:
        void DrawStats(RenderGraph& graph);
        void DrawPerformance();
        void DrawTimingSummary(const FrameTimings& timings, float budget);
        void DrawTimingHistory(const FrameHistory& history, float budget);
        void DrawTimingBreakdown(const FrameTimings& timings);
        void DrawTimingTable(const FrameTimings& timings);

    private:
        PanelCode mCode;
        RenderGraphTarget mTarget; // Shared by the Graph and Stats tabs
        RenderGraphVisualizer mVisualizer;
        Uint mBudgetIndex = 1; // Into BUDGET_RATES (RendererPanel.cpp), 60 Hz
    };

} // namespace Surge
