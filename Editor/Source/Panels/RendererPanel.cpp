// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Panels/RendererPanel.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Core/Time/Clock.hpp"
#include "Surge/Core/Diagnostics/Diagnostics.hpp"
#include "Surge/Graphics/RHI/RHI.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Utility/ImGuiAux.hpp"

#include <imgui.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <format>

namespace Surge
{
    namespace
    {
        constexpr std::array<float, 4> BUDGET_RATES = { 30.0f, 60.0f, 90.0f, 120.0f };
        constexpr std::array<const char*, 4> BUDGET_NAMES = { "30 Hz", "60 Hz", "90 Hz", "120 Hz" };

        constexpr ImU32 GPU_PLOT_COLOR = IM_COL32(100, 160, 255, 255);
        constexpr ImU32 CPU_PLOT_COLOR = IM_COL32(205, 205, 205, 200);
        constexpr ImU32 FRAME_PLOT_COLOR = IM_COL32(205, 205, 205, 80);
        constexpr ImVec4 GOOD_COLOR = ImVec4(0.4f, 0.85f, 0.5f, 1.0f);
        constexpr ImVec4 WARNING_COLOR = ImVec4(1.0f, 0.75f, 0.3f, 1.0f);
        constexpr ImVec4 OVER_COLOR = ImVec4(1.0f, 0.42f, 0.38f, 1.0f);

        // Fixed per group, so a group keeps its color in every view and every session
        ImU32 GroupColor(const String& name)
        {
            struct GroupColorEntry
            {
                const char* Name;
                ImU32 Color;
            };
            static constexpr std::array<GroupColorEntry, 7> GROUP_COLORS = { {
                { "Shadow", IM_COL32(146, 112, 202, 255) },
                { "MainScene", IM_COL32(91, 143, 249, 255) },
                { "AmbientOcclusion", IM_COL32(90, 216, 166, 255) },
                { "Bloom", IM_COL32(246, 189, 22, 255) },
                { "PostProcess", IM_COL32(232, 104, 74, 255) },
                { "UIOverlay", IM_COL32(109, 200, 236, 255) },
                { "Swapchain", IM_COL32(255, 157, 77, 255) },
            } };
            for(const GroupColorEntry& entry : GROUP_COLORS)
                if(name == entry.Name)
                    return entry.Color;

            // Unknown group: FNV-1a hashed hue
            uint32_t hash = 2166136261u;
            for(char c : name)
                hash = (hash ^ static_cast<uint8_t>(c)) * 16777619u;
            return ImColor::HSV(static_cast<float>(hash % 360u) / 360.0f, 0.5f, 0.9f);
        }

        // Called for every scope in pre-order: groups get their color, passes inherit their group's, graphs (depth 0) have none
        ImU32 ScopeColor(const DiagnosticsScope& scope, ImU32& groupColor)
        {
            if(scope.Depth == 1)
                groupColor = GroupColor(scope.Name);
            return scope.Depth == 0 ? 0 : groupColor;
        }

        ImU32 WithAlpha(ImU32 color, float alpha)
        {
            return (color & ~IM_COL32_A_MASK) | (static_cast<ImU32>(alpha * 255.0f) << IM_COL32_A_SHIFT);
        }

        // Green under 80% of the budget, amber up to it, red past it
        ImU32 BudgetColor(float milliseconds, float budget)
        {
            if(milliseconds <= budget * 0.8f)
                return ImGui::GetColorU32(GOOD_COLOR);
            return ImGui::GetColorU32(milliseconds <= budget ? WARNING_COLOR : OVER_COLOR);
        }

        ImFont* BoldFont()
        {
            ImFontAtlas* atlas = ImGui::GetIO().Fonts;
            return atlas->Fonts.Size > 1 ? atlas->Fonts[1] : ImGui::GetFont();
        }
    } // namespace

    void RendererPanel::Init(void*)
    {
        mCode = GetStaticCode();
    }

    void RendererPanel::Render(bool* show)
    {
        if(!*show)
            return;

        ImGui::SetNextWindowSize(ImVec2(420.0f, 640.0f), ImGuiCond_FirstUseEver);
        if(ImGui::Begin(PanelCodeToString(mCode), show))
        {
            if(ImGui::BeginTabBar("##RendererTabs"))
            {
                if(ImGui::BeginTabItem("RHI"))
                {
                    if(ImGui::BeginChild("##RHI"))
                        Core::GetRenderer()->GetRHI()->ShowMetricsWindow(Core::GetClock().GetMilliseconds());
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }

                if(ImGui::BeginTabItem("Graph"))
                {
                    mTarget.DrawSelector(std::min(ImGui::GetContentRegionAvail().x, 240.0f)); // Before Resolve(): a new pick applies this frame
                    mVisualizer.Draw(*mTarget.Resolve(), mTarget);
                    ImGui::EndTabItem();
                }

                if(ImGui::BeginTabItem("Stats"))
                {
                    mTarget.DrawSelector(std::min(ImGui::GetContentRegionAvail().x, 240.0f));
                    DrawStats(*mTarget.Resolve());
                    ImGui::EndTabItem();
                }

                if(ImGui::BeginTabItem("Performance"))
                {
                    DrawPerformance();
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }

    void RendererPanel::DrawStats(RenderGraph& graph)
    {
        if(mTarget.IsMainGraph() && !graph.IsSceneEnabled())
            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f), "The main graph only presents ImGui in the Editor, pick a viewport.");

        if(ImGui::BeginChild("##Stats"))
        {
            // Stats of the selected graph's own frame, so read its blackboard (submission counts differ per view)
            for(const Scope<RenderPass>& pass : graph.GetPasses())
            {
                if(!pass->IsEnabled())
                    continue;
                ImGui::PushID(pass.get());
                pass->RenderStatsViaImGui(graph.GetBlackboard());
                ImGui::PopID();
            }
        }
        ImGui::EndChild();
    }

    void RendererPanel::DrawPerformance()
    {
        Diagnostics* diagnostics = Core::GetDiagnostics();
        if(!diagnostics->SupportsGPUTiming())
        {
            ImGui::TextDisabled(RHISettings::GPU_TIMING ? "This GPU's queue has no timestamp support." : "GPU timing is compiled out (RHISettings::GPU_TIMING).");
            return;
        }

        const ImGuiStyle& style = ImGui::GetStyle();
        bool enabled = diagnostics->IsGPUTimingEnabled();
        if(ImGui::Checkbox("GPU Timing", &enabled))
            diagnostics->SetGPUTimingEnabled(enabled);
        ImGuiAux::ItemTooltip("Timestamp queries around every render graph group and pass, results arrive 2 frames late");

        // Frame budget everything is colored against, right aligned next to the switch
        const float comboWidth = ImGui::CalcTextSize("120 Hz").x + ImGui::GetFrameHeight() + style.FramePadding.x * 2.0f;
        const float labelWidth = ImGui::CalcTextSize("Target").x + style.ItemInnerSpacing.x;
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(ImGui::GetContentRegionAvail().x - comboWidth - labelWidth, 0.0f));
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Target");
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        ImGui::SetNextItemWidth(comboWidth);
        if(ImGui::BeginCombo("##Budget", BUDGET_NAMES[mBudgetIndex]))
        {
            for(Uint i = 0; i < BUDGET_RATES.size(); i++)
                if(ImGui::Selectable(BUDGET_NAMES[i], i == mBudgetIndex))
                    mBudgetIndex = i;
            ImGui::EndCombo();
        }
        ImGuiAux::ItemTooltip("Frame budget the timings are judged against: green under 80%, amber up to it, red past it");
        const float budget = 1000.0f / BUDGET_RATES[mBudgetIndex];

        const FrameTimings& timings = diagnostics->GetTimings();
        if(!enabled || !timings.GPUTiming)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("%s", enabled ? "Waiting for the first results..." : "Off: no queries are written or read back.");
            ImGui::PopStyleColor();
            return;
        }

        if(!Core::GetRenderer()->GetRHI()->SupportsGPUTimingInRenderPass())
            ImGui::TextColored(WARNING_COLOR, "Tile based GPU: passes sharing a render pass are timed together.");
        if(timings.GPUOverflowed)
            ImGui::TextColored(WARNING_COLOR, "More scopes than the query pool holds, the last ones were dropped.");

        ImGui::Spacing();
        DrawTimingSummary(timings, budget);
        ImGui::Spacing();
        DrawTimingHistory(diagnostics->GetHistory(), budget);
        ImGui::Spacing();
        DrawTimingBreakdown(timings);
        ImGui::Spacing();
        DrawTimingTable(timings);
    }

    void RendererPanel::DrawTimingSummary(const FrameTimings& timings, float budget)
    {
        // The most expensive scope without children (a pass, or a group when its passes aren't timed)
        const Vector<DiagnosticsScope>& scopes = timings.Scopes;
        const DiagnosticsScope* hottest = nullptr;
        const DiagnosticsScope* hottestGraph = nullptr;
        const DiagnosticsScope* graph = nullptr;
        ImU32 hottestColor = 0;
        ImU32 groupColor = 0;
        for(size_t i = 0; i < scopes.size(); i++)
        {
            const DiagnosticsScope& scope = scopes[i];
            const ImU32 color = ScopeColor(scope, groupColor);
            if(scope.Depth == 0)
                graph = &scope;
            const bool isLeaf = i + 1 == scopes.size() || scopes[i + 1].Depth <= scope.Depth;
            if(isLeaf && (!hottest || scope.AverageMs > hottest->AverageMs))
            {
                hottest = &scope;
                hottestGraph = graph;
                hottestColor = color;
            }
        }

        struct Card
        {
            const char* Label;
            String Value;
            String Detail;
            String Tooltip;
            ImU32 Accent;
            float Fill; // Bottom bar, negative for none
        };
        const float totalWork = std::max(timings.GPUWorkMs, 0.0001f);
        const float budgetUse = timings.GPUWorkMs / budget;
        const std::array<Card, 3> cards = { {
            { "GPU", std::format("{:.2f} ms", timings.GPUWorkMs), std::format("{:.0f}% of {:.1f} ms  peak {:.2f}", budgetUse * 100.0f, budget, timings.GPUWorkMaxMs),
              std::format("Every timed scope (views + main graph), averaged over {:.1f} s\nWaiting for the display (vsync, not work): {:.2f} ms\nWhole command buffer: {:.2f} ms", Diagnostics::WINDOW, timings.GPUDisplayWaitMs, timings.GPUCommandBufferMs),
              BudgetColor(timings.GPUWorkMs, budget), budgetUse },
            { "CPU", std::format("{:.2f} ms", timings.CPUWorkMs), std::format("frame {:.2f} ms  {:.0f} fps", timings.FrameMs, timings.FPS),
              std::format("The frame's own CPU work, without waiting on the GPU, the swapchain or the frame cap (peak {:.2f} ms)\nFrame time {:.2f} ms (peak {:.2f}) includes those waits", timings.CPUWorkMaxMs, timings.FrameMs, timings.FrameMaxMs),
              BudgetColor(timings.CPUWorkMs, budget), timings.CPUWorkMs / budget },
            { "Hottest", hottest ? hottest->Name : String("-"),
              hottest ? std::format("{:.2f} ms  {:.0f}%  {}", hottest->AverageMs, hottest->AverageMs / totalWork * 100.0f, hottestGraph ? hottestGraph->Name : String()) : String(),
              String("The most expensive scope without children, share of the GPU work"), hottestColor != 0 ? hottestColor : GPU_PLOT_COLOR, -1.0f },
        } };

        const ImGuiStyle& style = ImGui::GetStyle();
        ImFont* boldFont = BoldFont();
        const float valueSize = ImGui::GetFontSize() * 1.6f;
        const float lineHeight = ImGui::GetTextLineHeight();
        const ImVec2 padding = ImVec2(10.0f, 6.0f);
        constexpr float barHeight = 3.0f;
        const float barY = padding.y + lineHeight * 2.0f + valueSize + 4.0f;
        const float height = barY + barHeight + padding.y;
        const float width = std::max((ImGui::GetContentRegionAvail().x - style.ItemSpacing.x * 2.0f) / 3.0f, 1.0f);
        const ImVec2 origin = ImGui::GetCursorScreenPos();

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImU32 background = ImGui::GetColorU32(ImGuiCol_FrameBg);
        const ImU32 labelColor = ImGui::GetColorU32(ImGuiCol_TextDisabled);
        const ImU32 valueColor = ImGui::GetColorU32(ImGuiCol_Text);

        for(size_t i = 0; i < cards.size(); i++)
        {
            const Card& card = cards[i];
            const ImVec2 min = ImVec2(origin.x + (width + style.ItemSpacing.x) * static_cast<float>(i), origin.y);
            const ImVec2 max = ImVec2(min.x + width, min.y + height);
            drawList->AddRectFilled(min, max, background, style.FrameRounding);
            drawList->AddRectFilled(min, ImVec2(min.x + 3.0f, max.y), card.Accent, style.FrameRounding, ImDrawFlags_RoundCornersLeft);

            drawList->PushClipRect(min, max, true);
            const float x = min.x + padding.x;
            drawList->AddText(ImVec2(x, min.y + padding.y), labelColor, card.Label);
            drawList->AddText(boldFont, valueSize, ImVec2(x, min.y + padding.y + lineHeight), valueColor, card.Value.c_str());
            drawList->AddText(ImVec2(x, min.y + padding.y + lineHeight + valueSize), labelColor, card.Detail.c_str());
            if(card.Fill >= 0.0f)
            {
                const float barWidth = max.x - padding.x - x;
                drawList->AddRectFilled(ImVec2(x, min.y + barY), ImVec2(x + barWidth, min.y + barY + barHeight), WithAlpha(labelColor, 0.3f), barHeight);
                drawList->AddRectFilled(ImVec2(x, min.y + barY), ImVec2(x + barWidth * std::min(card.Fill, 1.0f), min.y + barY + barHeight), card.Accent, barHeight);
            }
            drawList->PopClipRect();

            ImGui::SetCursorScreenPos(min);
            ImGui::PushID(static_cast<int>(i));
            ImGui::InvisibleButton("##Card", ImVec2(width, height));
            ImGui::PopID();
            ImGui::SetItemTooltip("%s", card.Tooltip.c_str());
        }
        ImGui::SetCursorScreenPos(origin);
        ImGui::Dummy(ImVec2(width * 3.0f + style.ItemSpacing.x * 2.0f, height));
    }

    void RendererPanel::DrawTimingHistory(const FrameHistory& history, float budget)
    {
        const ImVec2 size = ImVec2(std::max(ImGui::GetContentRegionAvail().x, 1.0f), 96.0f);
        const ImVec2 min = ImGui::GetCursorScreenPos();
        const ImVec2 max = ImVec2(min.x + size.x, min.y + size.y);
        ImGui::InvisibleButton("##TimingHistory", size);
        const bool hovered = ImGui::IsItemHovered();

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(min, max, ImGui::GetColorU32(ImGuiCol_FrameBg), ImGui::GetStyle().FrameRounding);
        drawList->PushClipRect(min, max, true);

        // Newest sample on the right, the scale always keeps the budget line in view
        const Uint count = history.Count;
        const auto sample = [&history](const std::array<float, FrameHistory::SIZE>& values, Uint i) { return history.Sample(values, i); };
        float peak = 0.0f;
        for(Uint i = 0; i < count; i++)
            peak = std::max({ peak, history.GPUWorkMs[i], history.CPUWorkMs[i], history.FrameMs[i] });
        const float scaleMax = std::max(budget * 1.25f, peak * 1.1f);
        const float step = size.x / static_cast<float>(FrameHistory::SIZE - 1);
        const auto toX = [&](Uint i) { return max.x - static_cast<float>(count - 1 - i) * step; };
        const auto toY = [&](float milliseconds) { return max.y - std::min(milliseconds / scaleMax, 1.0f) * (size.y - 2.0f); };

        const float budgetY = toY(budget);
        const ImU32 budgetColor = WithAlpha(ImGui::GetColorU32(OVER_COLOR), 0.7f);
        drawList->AddLine(ImVec2(min.x, budgetY), ImVec2(max.x, budgetY), budgetColor, 1.0f);

        if(count > 1)
        {
            // GPU work as a filled area (no AA, adjacent quads would show seams) + outline, frame time and CPU work as lines on top
            const ImDrawListFlags drawListFlags = drawList->Flags;
            drawList->Flags &= ~ImDrawListFlags_AntiAliasedFill;
            const ImU32 gpuFill = WithAlpha(GPU_PLOT_COLOR, 0.25f);
            for(Uint i = 1; i < count; i++)
            {
                const float x0 = toX(i - 1);
                const float x1 = toX(i);
                drawList->AddQuadFilled(ImVec2(x0, max.y), ImVec2(x0, toY(sample(history.GPUWorkMs, i - 1))), ImVec2(x1, toY(sample(history.GPUWorkMs, i))), ImVec2(x1, max.y), gpuFill);
            }
            drawList->Flags = drawListFlags;

            std::array<ImVec2, FrameHistory::SIZE> points;
            const auto plot = [&](const std::array<float, FrameHistory::SIZE>& values, ImU32 color, float thickness) {
                for(Uint i = 0; i < count; i++)
                    points[i] = ImVec2(toX(i), toY(sample(values, i)));
                drawList->AddPolyline(points.data(), static_cast<int>(count), color, ImDrawFlags_None, thickness);
            };
            plot(history.GPUWorkMs, GPU_PLOT_COLOR, 1.5f);
            plot(history.FrameMs, FRAME_PLOT_COLOR, 1.0f);
            plot(history.CPUWorkMs, CPU_PLOT_COLOR, 1.0f);
        }

        // Legend (top left) and budget label (right, under its line)
        const float lineHeight = ImGui::GetTextLineHeight();
        const float swatch = std::floor(lineHeight * 0.5f);
        const ImU32 textColor = ImGui::GetColorU32(ImGuiCol_Text);
        float legendX = min.x + 6.0f;
        const float legendY = min.y + 4.0f;
        for(const auto& [color, label] : { std::pair { GPU_PLOT_COLOR, "GPU" }, std::pair { CPU_PLOT_COLOR, "CPU" }, std::pair { FRAME_PLOT_COLOR, "Frame" } })
        {
            const float swatchY = legendY + (lineHeight - swatch) * 0.5f;
            drawList->AddRectFilled(ImVec2(legendX, swatchY), ImVec2(legendX + swatch, swatchY + swatch), color, 2.0f);
            legendX += swatch + 4.0f;
            drawList->AddText(ImVec2(legendX, legendY), textColor, label);
            legendX += ImGui::CalcTextSize(label).x + 10.0f;
        }
        char budgetLabel[32];
        std::snprintf(budgetLabel, sizeof(budgetLabel), "%.1f ms", budget);
        drawList->AddText(ImVec2(max.x - ImGui::CalcTextSize(budgetLabel).x - 6.0f, budgetY + 2.0f), budgetColor, budgetLabel);

        if(hovered && count > 0)
        {
            const int fromRight = static_cast<int>(std::round((max.x - ImGui::GetIO().MousePos.x) / step));
            if(fromRight >= 0 && fromRight < static_cast<int>(count))
            {
                const Uint i = count - 1 - static_cast<Uint>(fromRight);
                const float x = toX(i);
                drawList->AddLine(ImVec2(x, min.y), ImVec2(x, max.y), WithAlpha(textColor, 0.4f), 1.0f);
                drawList->AddCircleFilled(ImVec2(x, toY(sample(history.GPUWorkMs, i))), 3.0f, GPU_PLOT_COLOR);
                ImGui::SetTooltip("%d frames ago\nGPU %.2f ms\nCPU %.2f ms\nFrame %.2f ms", fromRight, sample(history.GPUWorkMs, i),
                                  sample(history.CPUWorkMs, i), sample(history.FrameMs, i));
            }
        }
        drawList->PopClipRect();
    }

    void RendererPanel::DrawTimingBreakdown(const FrameTimings& timings)
    {
        const ImVec2 size = ImVec2(std::max(ImGui::GetContentRegionAvail().x, 1.0f), ImGui::GetFrameHeight());
        const ImVec2 min = ImGui::GetCursorScreenPos();
        const ImVec2 max = ImVec2(min.x + size.x, min.y + size.y);
        ImGui::InvisibleButton("##TimingBreakdown", size);
        const bool hovered = ImGui::IsItemHovered();
        const float mouseX = ImGui::GetIO().MousePos.x;

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const float rounding = ImGui::GetStyle().FrameRounding;
        drawList->AddRectFilled(min, max, ImGui::GetColorU32(ImGuiCol_FrameBg), rounding);
        drawList->PushClipRect(min, max, true);

        // One segment per group of every graph in execution order (a graph without groups is one segment),
        // graphs separated by a gap. Time a graph spends outside its groups (barriers, uploads) stays background.
        const Vector<DiagnosticsScope>& scopes = timings.Scopes;
        const float total = std::max(timings.GPUWorkMs, 0.0001f);
        const ImU32 separatorColor = ImGui::GetColorU32(ImGuiCol_WindowBg);
        const ImU32 labelColor = IM_COL32(20, 20, 24, 255);
        const DiagnosticsScope* graph = nullptr;
        const DiagnosticsScope* hoveredRow = nullptr;
        const DiagnosticsScope* hoveredGraph = nullptr;
        ImU32 groupColor = 0;
        float x = min.x;
        for(size_t i = 0; i < scopes.size(); i++)
        {
            const DiagnosticsScope& row = scopes[i];
            const ImU32 rowColor = ScopeColor(row, groupColor);
            const bool hasChildren = i + 1 < scopes.size() && scopes[i + 1].Depth > row.Depth;
            if(row.Depth == 0)
            {
                if(x > min.x)
                    drawList->AddLine(ImVec2(x, min.y), ImVec2(x, max.y), separatorColor, 2.0f);
                graph = &row;
                if(hasChildren)
                    continue;
            }
            else if(row.Depth != 1)
                continue;

            const float width = size.x * std::clamp(row.AverageMs / total, 0.0f, 1.0f);
            const bool isHovered = hovered && mouseX >= x && mouseX < x + width;
            const ImU32 color = rowColor != 0 ? rowColor : GPU_PLOT_COLOR;
            drawList->AddRectFilled(ImVec2(x, min.y), ImVec2(x + width, max.y), isHovered ? color : WithAlpha(color, 0.8f), rounding, x == min.x ? ImDrawFlags_RoundCornersLeft : ImDrawFlags_RoundCornersNone);

            const ImVec2 textSize = ImGui::CalcTextSize(row.Name.c_str());
            if(textSize.x + 8.0f <= width)
                drawList->AddText(ImVec2(x + 4.0f, min.y + (size.y - textSize.y) * 0.5f), labelColor, row.Name.c_str());
            if(isHovered)
            {
                hoveredRow = &row;
                hoveredGraph = graph;
            }
            x += width;
        }
        drawList->PopClipRect();

        if(hoveredRow)
        {
            if(hoveredGraph && hoveredGraph != hoveredRow)
                ImGui::SetTooltip("%s > %s\n%.2f ms, %.1f%% of the GPU work", hoveredGraph->Name.c_str(), hoveredRow->Name.c_str(), hoveredRow->AverageMs, hoveredRow->AverageMs / total * 100.0f);
            else
                ImGui::SetTooltip("%s\n%.2f ms, %.1f%% of the GPU work", hoveredRow->Name.c_str(), hoveredRow->AverageMs, hoveredRow->AverageMs / total * 100.0f);
        }
    }

    void RendererPanel::DrawTimingTable(const FrameTimings& timings)
    {
        constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;
        if(!ImGui::BeginTable("##GPUTimings", 4, tableFlags))
            return;

        const float numberWidth = ImGui::CalcTextSize("Avg msex").x;
        const float percentWidth = ImGui::CalcTextSize("100.0%").x;
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Scope", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Avg ms", ImGuiTableColumnFlags_WidthFixed, numberWidth);
        ImGui::TableSetupColumn("Max ms", ImGuiTableColumnFlags_WidthFixed, numberWidth);
        ImGui::TableSetupColumn("Share", ImGuiTableColumnFlags_WidthFixed, percentWidth * 2.2f);
        ImGui::TableHeadersRow();

        const ImGuiStyle& style = ImGui::GetStyle();
        const Vector<DiagnosticsScope>& scopes = timings.Scopes;
        const float total = std::max(timings.GPUWorkMs, 0.0001f);
        const float indent = style.IndentSpacing * 0.75f;
        const float lineHeight = ImGui::GetTextLineHeight();
        const float swatchSize = std::floor(lineHeight * 0.6f);
        ImFont* boldFont = BoldFont();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImU32 graphRowBackground = ImGui::GetColorU32(ImGuiCol_TableHeaderBg);
        const ImU32 barBackground = ImGui::GetColorU32(ImGuiCol_FrameBg);
        const ImU32 textColor = ImGui::GetColorU32(ImGuiCol_Text);

        // Rows are pre-order: a closed node hides the deeper rows that follow it
        Uint hiddenBelow = ~0u;
        ImU32 groupColor = 0;
        for(size_t i = 0; i < scopes.size(); i++)
        {
            const DiagnosticsScope& row = scopes[i];
            const ImU32 rowColor = ScopeColor(row, groupColor); // Before skipping: a hidden group still sets its passes' color
            if(row.Depth > hiddenBelow)
                continue;
            hiddenBelow = ~0u;

            const bool hasChildren = i + 1 < scopes.size() && scopes[i + 1].Depth > row.Depth;
            const float share = std::clamp(row.AverageMs / total, 0.0f, 1.0f);
            ImGui::PushID(static_cast<int>(i));
            ImGui::TableNextRow();
            if(row.Depth == 0)
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, graphRowBackground);

            ImGui::TableSetColumnIndex(0);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent * static_cast<float>(row.Depth));
            if(rowColor != 0) // Same color as the breakdown bar, passes dimmed
            {
                const ImVec2 pos = ImGui::GetCursorScreenPos();
                const float y = pos.y + (lineHeight - swatchSize) * 0.5f;
                drawList->AddRectFilled(ImVec2(pos.x, y), ImVec2(pos.x + swatchSize, y + swatchSize), row.Depth == 1 ? rowColor : WithAlpha(rowColor, 0.45f), 2.0f);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + swatchSize + style.ItemInnerSpacing.x);
            }
            ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
            if(!hasChildren)
                nodeFlags |= ImGuiTreeNodeFlags_Leaf;
            if(row.Depth == 0)
                ImGui::PushFont(boldFont, 0.0f);
            const bool open = ImGui::TreeNodeEx("##Scope", nodeFlags, "%s", row.Name.c_str());
            if(row.Depth == 0)
                ImGui::PopFont();
            if(!open && hasChildren)
                hiddenBelow = row.Depth;

            // Heat: the scopes that eat the frame stand out
            ImGui::TableSetColumnIndex(1);
            if(row.Depth > 0 && share >= 0.25f)
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.35f, 1.0f), "%.2f", row.AverageMs);
            else if(row.Depth > 0 && share >= 0.1f)
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.5f, 1.0f), "%.2f", row.AverageMs);
            else
                ImGui::Text("%.2f", row.AverageMs);

            ImGui::TableSetColumnIndex(2);
            if(row.MaxMs > row.AverageMs * 2.0f && row.MaxMs - row.AverageMs > 0.25f)
            {
                ImGui::TextColored(WARNING_COLOR, "%.2f", row.MaxMs);
                ImGui::SetItemTooltip("Spiky: the peak is over twice the average");
            }
            else
                ImGui::TextDisabled("%.2f", row.MaxMs);

            // Share of the GPU work (not the GPU frame, which includes the swapchain wait)
            ImGui::TableSetColumnIndex(3);
            char percent[16];
            std::snprintf(percent, sizeof(percent), "%.1f%%", share * 100.0f);
            const ImVec2 min = ImGui::GetCursorScreenPos();
            const ImVec2 size = ImVec2(ImGui::GetContentRegionAvail().x, lineHeight);
            const float barWidth = std::max(size.x - percentWidth - style.ItemInnerSpacing.x, 0.0f);
            const ImU32 barColor = rowColor != 0 ? rowColor : GPU_PLOT_COLOR;
            drawList->AddRectFilled(ImVec2(min.x, min.y + 3.0f), ImVec2(min.x + barWidth, min.y + size.y - 3.0f), barBackground, 2.0f);
            drawList->AddRectFilled(ImVec2(min.x, min.y + 3.0f), ImVec2(min.x + barWidth * share, min.y + size.y - 3.0f), row.Depth <= 1 ? barColor : WithAlpha(barColor, 0.6f), 2.0f);
            drawList->AddText(ImVec2(min.x + size.x - ImGui::CalcTextSize(percent).x, min.y), textColor, percent);
            ImGui::Dummy(size);

            ImGui::PopID();
        }
        ImGui::EndTable();
    }

} // namespace Surge
