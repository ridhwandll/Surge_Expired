// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Panels/RenderGraphVisualizer.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Graphics/RHI/RHI.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"

#include <algorithm>
#include <format>

namespace Surge
{
    namespace
    {
        constexpr float MARGIN = 12.0f;
        constexpr float GROUP_MIN_WIDTH = 210.0f;
        constexpr float GROUP_GAP = 46.0f;
        constexpr float HEADER_HEIGHT = 28.0f;
        constexpr float HEADER_FONT_SIZE = 17.0f;
        constexpr float PASS_MIN_HEIGHT = 24.0f;
        constexpr float PASS_GAP = 4.0f;
        constexpr float PAD_X = 8.0f;
        constexpr float PAD_Y = 8.0f;
        constexpr float ROUNDING = 5.0f;
        constexpr float LANE_GAP = 18.0f;
        constexpr float PORT_INSET = 10.0f;
        constexpr float PORT_GAP = 8.0f;
        constexpr float ARROW_LENGTH = 9.0f;
        constexpr float ARROW_HALF_WIDTH = 5.0f;
        constexpr float EDGE_HOVER_DISTANCE = 5.0f;
        constexpr float GRID_STEP = 24.0f;

        ImFont* BoldFont()
        {
            const ImGuiIO& io = ImGui::GetIO();
            return io.Fonts->Fonts.Size > 1 ? io.Fonts->Fonts[1] : nullptr; // nullptr keeps the current font in PushFont()
        }

        bool Contains(const Vector<ImageHandle>& list, ImageHandle h) { return std::find(list.begin(), list.end(), h) != list.end(); }

        ImU32 GroupColor(PassGroup type)
        {
            switch(type)
            {
                case PassGroup::SHADOW:            return IM_COL32(65, 145, 75, 255);
                case PassGroup::MAIN_SCENE:        return IM_COL32(95, 115, 140, 255);
                case PassGroup::AMBIENT_OCCLUSION: return IM_COL32(95, 105, 180, 255);
                case PassGroup::BLOOM:             return IM_COL32(200, 105, 160, 255);
                case PassGroup::POST_PROCESS:      return IM_COL32(190, 95, 80, 255);
                case PassGroup::UI_OVERLAY:        return IM_COL32(60, 150, 150, 255);
                case PassGroup::SWAPCHAIN:         return IM_COL32(175, 140, 45, 255);
                default:                           return IM_COL32(115, 125, 115, 255);
            }
        }

        ImVec2 FitImage(const ImageDesc& desc, float maxDim)
        {
            const float aspect = static_cast<float>(desc.Width) / static_cast<float>(std::max(desc.Height, 1u));
            return aspect >= 1.0f ? ImVec2(maxDim, maxDim / aspect) : ImVec2(maxDim * aspect, maxDim);
        }

        float DistToSegmentSq(const ImVec2& p, const ImVec2& a, const ImVec2& b)
        {
            const float abX = b.x - a.x, abY = b.y - a.y;
            const float apX = p.x - a.x, apY = p.y - a.y;
            const float lenSq = abX * abX + abY * abY;
            float t = lenSq > 0.0f ? (apX * abX + apY * abY) / lenSq : 0.0f;
            t = std::clamp(t, 0.0f, 1.0f);
            const float dx = apX - abX * t, dy = apY - abY * t;
            return dx * dx + dy * dy;
        }

        // "SAMPLED | COLOR_ATTACHMENT", the Vulkan helper for this isn't visible to the Editor
        const char* UsageToString(ImageUsage usage, char (&buffer)[160])
        {
            struct Flag
            {
                ImageUsage Bit;
                const char* Name;
            };
            constexpr Flag flags[] = {
                { ImageUsage::SAMPLED, "SAMPLED" },
                { ImageUsage::COLOR_ATTACHMENT, "COLOR_ATTACHMENT" },
                { ImageUsage::DEPTH_ATTACHMENT, "DEPTH_ATTACHMENT" },
                { ImageUsage::TRANSIENT_ATTACHMENT, "TRANSIENT_ATTACHMENT" },
                { ImageUsage::STORAGE, "STORAGE" },
                { ImageUsage::TRANSFER_SRC, "TRANSFER_SRC" },
                { ImageUsage::TRANSFER_DST, "TRANSFER_DST" },
            };

            char* out = buffer;
            const char* end = buffer + sizeof(buffer) - 1;
            for(const Flag& flag : flags)
            {
                if(!(usage & flag.Bit))
                    continue;

                if(out != buffer)
                    out = std::format_to_n(out, end - out, " | ").out;
                out = std::format_to_n(out, end - out, "{}", flag.Name).out;
            }
            *out = '\0';
            return out == buffer ? "NONE" : buffer;
        }
    } // namespace

    const char* RenderGraphVisualizer::ImageName(ImageHandle h) const
    {
        // Barriers and pass lists may hold handles of images a pass destroyed since Compile(), never look them up
        if(!mRHI->IsValid(h))
            return "<Destroyed Image>";
        const String& name = mRHI->GetDesc(h).DebugName;
        return name.empty() ? "<Unnamed Image>" : name.c_str();
    }

    RenderPass* RenderGraphVisualizer::FindSelectedPass(RenderGraph& graph, size_t& outGroup, int& outOrder) const
    {
        if(mSelectedPass.empty())
            return nullptr;

        const Vector<ExecutionGroup>& groups = graph.GetCompiledGraph().Groups;
        int order = 0;
        for(size_t groupIndex = 0; groupIndex < groups.size(); groupIndex++)
        {
            for(RenderPass* pass : groups[groupIndex].Passes)
            {
                if(pass->GetName() == mSelectedPass)
                {
                    outGroup = groupIndex;
                    outOrder = order;
                    return pass;
                }
                order++;
            }
        }
        return nullptr;
    }

    void RenderGraphVisualizer::SetPassEnabled(RenderPass* pass, bool enabled)
    {
        if(!mLinkToggles)
        {
            pass->SetEnabled(enabled);
            return;
        }

        // Every graph runs the same passes (names are unique per graph): one toggle for all viewports
        const String name = pass->GetName();
        const auto apply = [&](RenderGraph& graph) {
            for(const Scope<RenderPass>& other : graph.GetPasses())
            {
                if(other->GetName() == name)
                    other->SetEnabled(enabled);
            }
        };

        Renderer* renderer = Core::GetRenderer();
        apply(renderer->GetMainRenderGraph());
        renderer->ForEachView([&](RenderViewID id, const RenderViewDesc&) {
            if(RenderGraph* viewGraph = renderer->GetViewGraph(id))
                apply(*viewGraph);
        });
    }

    void RenderGraphVisualizer::Draw(RenderGraph& graph, const RenderGraphTarget& target)
    {
        mRHI = graph.GetRHI();
        if(!mRHI || !graph.GetCompiledGraph().IsValid || graph.GetCompiledGraph().Groups.empty())
        {
            ImGui::TextDisabled("This RenderGraph has not been compiled yet.");
            return;
        }

        DrawToolbar(graph, target);

        // Side by side when there is room, otherwise the inspector goes under the graph (narrow docks)
        const ImVec2 available = ImGui::GetContentRegionAvail();
        if(available.x >= 640.0f)
        {
            if(ImGui::BeginTable("##RenderGraphLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV, available))
            {
                ImGui::TableSetupColumn("Graph", ImGuiTableColumnFlags_WidthStretch, 0.6f);
                ImGui::TableSetupColumn("Inspector", ImGuiTableColumnFlags_WidthStretch, 0.4f);
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                if(ImGui::BeginChild("##GraphCanvas", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
                    DrawGraph(graph);
                ImGui::EndChild();

                ImGui::TableSetColumnIndex(1);
                if(ImGui::BeginChild("##PassInspector", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
                    DrawInspector(graph);
                ImGui::EndChild();

                ImGui::EndTable();
            }
        }
        else
        {
            if(ImGui::BeginChild("##GraphCanvas", ImVec2(0.0f, std::max(available.y * 0.55f, 120.0f)), ImGuiChildFlags_ResizeY, ImGuiWindowFlags_HorizontalScrollbar))
                DrawGraph(graph);
            ImGui::EndChild();

            if(ImGui::BeginChild("##PassInspector", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
                DrawInspector(graph);
            ImGui::EndChild();
        }
    }

    void RenderGraphVisualizer::DrawToolbar(RenderGraph& graph, const RenderGraphTarget& target)
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        const Vector<ExecutionGroup>& groups = graph.GetCompiledGraph().Groups;

        Uint passCount = 0, enabledCount = 0, barrierCount = 0;
        for(const ExecutionGroup& group : groups)
        {
            barrierCount += static_cast<Uint>(group.BarriersBeforeGroup.size());
            for(RenderPass* pass : group.Passes)
            {
                passCount++;
                enabledCount += pass->IsEnabled() ? 1 : 0;
            }
        }

        // Right of the target selector when it fits, own row otherwise
        const char* enableAllLabel = "Enable All";
        const char* linkLabel = "All Viewports";
        const float buttonWidth = ImGui::CalcTextSize(enableAllLabel).x + style.FramePadding.x * 2.0f;
        const float linkWidth = ImGui::GetFrameHeight() + style.ItemInnerSpacing.x + ImGui::CalcTextSize(linkLabel).x;
        const float needed = linkWidth + style.ItemSpacing.x + buttonWidth;
        ImGui::SameLine();
        if(ImGui::GetContentRegionAvail().x < needed)
            ImGui::NewLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(ImGui::GetContentRegionAvail().x - needed, 0.0f));

        ImGui::Checkbox(linkLabel, &mLinkToggles);
        ImGui::SetItemTooltip("Enabling or disabling a pass applies to every viewport's graph");
        ImGui::SameLine();

        ImGui::BeginDisabled(enabledCount == passCount);
        if(ImGui::Button(enableAllLabel))
        {
            for(const ExecutionGroup& group : groups)
            {
                for(RenderPass* pass : group.Passes)
                    SetPassEnabled(pass, true);
            }
        }
        ImGui::EndDisabled();

        const FrameBlackboard& blackboard = graph.GetBlackboard();
        ImGui::TextDisabled("%u x %u  |  %u groups  |  %u/%u passes  |  %u barriers", static_cast<Uint>(blackboard.ScreenWidth), static_cast<Uint>(blackboard.ScreenHeight),
                            static_cast<Uint>(groups.size()), enabledCount, passCount, barrierCount);

        const ImVec4 warningColor(1.0f, 0.75f, 0.3f, 1.0f);
        if(target.IsMainGraph() && !graph.IsSceneEnabled())
            ImGui::TextColored(warningColor, "Scene groups skipped: in the Editor the main graph only presents ImGui.");
        else if(!target.IsMainGraph() && !Core::GetRenderer()->HasViewOutput(target.GetViewID()))
            ImGui::TextColored(warningColor, "Not drawn yet (hidden tab, no camera or just resized).");
        ImGui::Separator();
    }

    void RenderGraphVisualizer::DrawGraph(RenderGraph& graph)
    {
        const ImGuiIO& io = ImGui::GetIO();
        ImFont* boldFont = BoldFont();
        const Vector<ExecutionGroup>& groups = graph.GetCompiledGraph().Groups;
        const size_t groupCount = groups.size();
        const bool sceneSkipped = !graph.IsSceneEnabled();

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 canvasPos = ImGui::GetCursorScreenPos(); // Already accounts for scrolling
        const float lineHeight = ImGui::GetTextLineHeight();
        const float checkSize = ImGui::GetFrameHeight();
        const float passHeight = std::max(PASS_MIN_HEIGHT, checkSize + 4.0f);
        const float orderColumnWidth = ImGui::CalcTextSize("00").x + 6.0f;

        // Uniform group width that fits the longest label
        float groupWidth = GROUP_MIN_WIDTH;
        ImGui::PushFont(boldFont, HEADER_FONT_SIZE);
        for(const ExecutionGroup& group : groups)
            groupWidth = std::max(groupWidth, ImGui::CalcTextSize(group.Name.c_str()).x + PAD_X * 4.0f);
        ImGui::PopFont();

        for(const ExecutionGroup& group : groups)
        {
            for(const RenderPass* pass : group.Passes)
                groupWidth = std::max(groupWidth, PAD_X * 2.0f + 6.0f + orderColumnWidth + ImGui::CalcTextSize(pass->GetName().c_str()).x + 8.0f + checkSize + 6.0f);

            for(const ImageBarrier& barrier : group.BarriersBeforeGroup)
                groupWidth = std::max(groupWidth, PAD_X * 2.0f + 14.0f + ImGui::CalcTextSize(ImageName(barrier.Handle)).x);
        }

        // Vertical layout in execution order
        mLayouts.resize(groupCount);
        {
            const float x = canvasPos.x + MARGIN;
            float y = canvasPos.y + MARGIN;
            for(size_t groupIndex = 0; groupIndex < groupCount; ++groupIndex)
            {
                const ExecutionGroup& group = groups[groupIndex];
                float height = HEADER_HEIGHT + PAD_Y;
                if(!group.BarriersBeforeGroup.empty())
                    height += lineHeight + 2.0f + static_cast<float>(group.BarriersBeforeGroup.size()) * lineHeight + PAD_Y;

                const size_t rows = std::max<size_t>(group.Passes.size(), 1);
                height += static_cast<float>(rows) * passHeight + static_cast<float>(rows - 1) * PASS_GAP + PAD_Y;

                mLayouts[groupIndex] = { ImVec2(x, y), ImVec2(x + groupWidth, y + height) };
                y += height + GROUP_GAP;
            }
        }

        // Dependency edges: every barrier comes from the most recent group (executed before the reader) that writes the image
        mEdges.clear();
        mEdgeImages.clear();
        const auto producerGroup = [&](size_t dst, ImageHandle h) -> size_t {
            for(size_t i = dst; i-- > 0;)
            {
                for(const RenderPass* pass : groups[i].Passes)
                {
                    if(Contains(pass->GetImageWrites(), h))
                        return i;
                }
            }
            return SIZE_MAX;
        };

        for(size_t dst = 0; dst < groupCount; ++dst)
        {
            for(size_t src = dst; src-- > 0;)
            {
                const size_t first = mEdgeImages.size();
                for(const ImageBarrier& barrier : groups[dst].BarriersBeforeGroup)
                {
                    if(producerGroup(dst, barrier.Handle) != src)
                        continue;

                    bool duplicate = false;
                    for(size_t k = first; k < mEdgeImages.size() && !duplicate; ++k)
                        duplicate = mEdgeImages[k].Handle == barrier.Handle;

                    if(!duplicate)
                        mEdgeImages.push_back(barrier);
                }

                if(mEdgeImages.size() > first)
                    mEdges.push_back({ src, dst, first, mEdgeImages.size() - first, -1, 0.0f, 0.0f });
            }
        }

        // Edges that skip groups are routed through lanes on the right, shortest spans get the innermost lanes
        int laneCount = 0;
        for(size_t span = 2; span < groupCount; ++span)
        {
            for(GraphEdge& edge : mEdges)
            {
                if(edge.Dst - edge.Src != span)
                    continue;

                int lane = 0;
                for(bool taken = true; taken;)
                {
                    taken = false;
                    for(const GraphEdge& other : mEdges)
                    {
                        if(&other != &edge && other.Lane == lane && other.Src < edge.Dst && edge.Src < other.Dst)
                        {
                            taken = true;
                            lane++;
                            break;
                        }
                    }
                }
                edge.Lane = lane;
                laneCount = std::max(laneCount, lane + 1);
            }
        }

        // Ports on the group's right side. Inner lanes leave lowest and enter highest, so lane segments never cross each other
        for(GraphEdge& edge : mEdges)
        {
            if(edge.Lane < 0)
                continue;

            int outSlot = 0, inSlot = 0;
            for(const GraphEdge& other : mEdges)
            {
                if(&other == &edge || other.Lane < 0 || other.Lane >= edge.Lane)
                    continue;
                outSlot += (other.Src == edge.Src) ? 1 : 0;
                inSlot += (other.Dst == edge.Dst) ? 1 : 0;
            }

            const GroupLayout& src = mLayouts[edge.Src];
            const GroupLayout& dst = mLayouts[edge.Dst];
            edge.ExitY = std::max(src.Max.y - PORT_INSET - static_cast<float>(outSlot) * PORT_GAP, src.Min.y + HEADER_HEIGHT);
            edge.EntryY = std::min(dst.Min.y + HEADER_HEIGHT * 0.5f + static_cast<float>(inSlot) * PORT_GAP, dst.Max.y - PORT_INSET);
        }

        const auto edgePoints = [&](const GraphEdge& edge, ImVec2* points) -> int {
            const GroupLayout& src = mLayouts[edge.Src];
            const GroupLayout& dst = mLayouts[edge.Dst];
            if(edge.Lane < 0)
            {
                const float centerX = (src.Min.x + src.Max.x) * 0.5f;
                points[0] = ImVec2(centerX, src.Max.y);
                points[1] = ImVec2(centerX, dst.Min.y);
                return 2;
            }

            const float laneX = src.Max.x + LANE_GAP * static_cast<float>(edge.Lane + 1);
            points[0] = ImVec2(src.Max.x, edge.ExitY);
            points[1] = ImVec2(laneX, edge.ExitY);
            points[2] = ImVec2(laneX, edge.EntryY);
            points[3] = ImVec2(dst.Max.x, edge.EntryY);
            return 4;
        };

        const bool canvasHovered = ImGui::IsWindowHovered();

        // Background grid (visible region only)
        {
            const ImVec2 windowMin = ImGui::GetWindowPos();
            const ImVec2 windowMax(windowMin.x + ImGui::GetWindowSize().x, windowMin.y + ImGui::GetWindowSize().y);
            const ImU32 gridColor = IM_COL32(255, 255, 255, 10);
            const float startX = canvasPos.x + static_cast<float>(static_cast<int>((windowMin.x - canvasPos.x) / GRID_STEP) - 1) * GRID_STEP;
            const float startY = canvasPos.y + static_cast<float>(static_cast<int>((windowMin.y - canvasPos.y) / GRID_STEP) - 1) * GRID_STEP;
            for(float gridX = startX; gridX < windowMax.x; gridX += GRID_STEP)
                drawList->AddLine(ImVec2(gridX, windowMin.y), ImVec2(gridX, windowMax.y), gridColor);
            for(float gridY = startY; gridY < windowMax.y; gridY += GRID_STEP)
                drawList->AddLine(ImVec2(windowMin.x, gridY), ImVec2(windowMax.x, gridY), gridColor);
        }

        // Execution groups
        size_t selectedGroup = SIZE_MAX;
        int unusedOrder = 0;
        RenderPass* selectedPass = FindSelectedPass(graph, selectedGroup, unusedOrder);

        int executionOrder = 0;
        for(size_t groupIndex = 0; groupIndex < groupCount; ++groupIndex)
        {
            const ExecutionGroup& group = groups[groupIndex];
            const GroupLayout& layout = mLayouts[groupIndex];
            const bool skipped = sceneSkipped && !group.IsSwapchain; // RenderGraph::SetSceneEnabled(false)
            const ImU32 color = skipped ? IM_COL32(85, 85, 85, 255) : GroupColor(group.Type);
            const ImVec2 headerMax(layout.Max.x, layout.Min.y + HEADER_HEIGHT);

            drawList->AddRectFilled(layout.Min, layout.Max, IM_COL32(18, 18, 18, 235), ROUNDING);
            drawList->AddRectFilled(layout.Min, headerMax, color, ROUNDING, ImDrawFlags_RoundCornersTop);
            drawList->AddRect(layout.Min, layout.Max, color, ROUNDING, 0, groupIndex == selectedGroup ? 2.5f : 1.5f);

            ImGui::PushFont(boldFont, HEADER_FONT_SIZE);
            char header[96];
            *std::format_to_n(header, sizeof(header) - 1, "{}{}", group.Name, skipped ? " (Skipped)" : "").out = '\0';
            const ImVec2 nameSize = ImGui::CalcTextSize(header);
            drawList->AddText(ImVec2(std::floor(layout.Min.x + (groupWidth - nameSize.x) * 0.5f), std::floor(layout.Min.y + (HEADER_HEIGHT - nameSize.y) * 0.5f)), IM_COL32(10, 10, 10, 255), header);
            ImGui::PopFont();

            if(canvasHovered && ImGui::IsMouseHoveringRect(layout.Min, headerMax) && ImGui::BeginTooltip())
            {
                ImGui::TextUnformatted(group.Name.c_str());
                ImGui::Separator();
                ImGui::Text("Passes: %u", static_cast<Uint>(group.Passes.size()));
                ImGui::Text("Barriers before group: %u", static_cast<Uint>(group.BarriersBeforeGroup.size()));
                ImGui::Text("Render pass: %s", group.IsSwapchain ? "Swapchain" : (group.ManagesOwnExecution ? "Managed by its passes" : "Begun by RenderGraph"));
                if(skipped)
                    ImGui::TextDisabled("Skipped: the scene is disabled on this graph");
                ImGui::EndTooltip();
            }

            float y = layout.Min.y + HEADER_HEIGHT + PAD_Y;

            // Barriers recorded before this group begins
            if(!group.BarriersBeforeGroup.empty())
            {
                drawList->AddText(ImVec2(layout.Min.x + PAD_X, y), IM_COL32(255, 195, 70, 255), "BARRIERS");
                y += lineHeight + 2.0f;
                for(const ImageBarrier& barrier : group.BarriersBeforeGroup)
                {
                    drawList->AddCircleFilled(ImVec2(layout.Min.x + PAD_X + 4.0f, y + lineHeight * 0.5f), 2.5f, IM_COL32(255, 195, 70, 255));
                    drawList->AddText(ImVec2(layout.Min.x + PAD_X + 14.0f, y), IM_COL32(220, 220, 220, 255), ImageName(barrier.Handle));
                    y += lineHeight;
                }
                drawList->AddLine(ImVec2(layout.Min.x + PAD_X, y + PAD_Y * 0.5f), ImVec2(layout.Max.x - PAD_X, y + PAD_Y * 0.5f), IM_COL32(80, 80, 80, 200));
                y += PAD_Y;
            }

            if(group.Passes.empty())
            {
                const char* emptyText = "(No passes)";
                const ImVec2 textSize = ImGui::CalcTextSize(emptyText);
                drawList->AddText(ImVec2(layout.Min.x + (groupWidth - textSize.x) * 0.5f, y + (passHeight - textSize.y) * 0.5f), ImGui::GetColorU32(ImGuiCol_TextDisabled), emptyText);
            }

            for(RenderPass* pass : group.Passes)
            {
                bool enabled = pass->IsEnabled();
                const ImVec2 passMin(layout.Min.x + PAD_X, y);
                const ImVec2 passMax(layout.Max.x - PAD_X, y + passHeight);
                const ImVec2 checkPos(passMax.x - checkSize - 3.0f, passMin.y + (passHeight - checkSize) * 0.5f);

                ImGui::PushID(pass);

                // Whole row (minus the checkbox) selects the pass
                ImGui::SetCursorScreenPos(passMin);
                if(ImGui::InvisibleButton("##Pass", ImVec2(std::max(checkPos.x - passMin.x - 2.0f, 1.0f), passHeight)))
                {
                    mSelectedPass = pass->GetName();
                    selectedPass = pass;
                    selectedGroup = groupIndex;
                }
                const bool hovered = ImGui::IsItemHovered();
                if(ImGui::BeginItemTooltip())
                {
                    ImGui::TextUnformatted(pass->GetName().c_str());
                    ImGui::TextDisabled("Reads: %u  |  Writes: %u", static_cast<Uint>(pass->GetImageReads().size()), static_cast<Uint>(pass->GetImageWrites().size()));
                    ImGui::TextDisabled("Click to inspect");
                    ImGui::EndTooltip();
                }

                const bool selected = pass == selectedPass;
                const ImU32 fillColor = selected ? IM_COL32(72, 52, 28, 255) : hovered ? IM_COL32(64, 64, 64, 255) : enabled ? IM_COL32(48, 48, 48, 255) : IM_COL32(62, 28, 28, 255);
                const ImU32 borderColor = selected ? IM_COL32(255, 153, 26, 255) : hovered ? color : enabled ? IM_COL32(80, 80, 80, 255) : IM_COL32(150, 55, 55, 255);
                drawList->AddRectFilled(passMin, passMax, fillColor, 4.0f);
                drawList->AddRect(passMin, passMax, borderColor, 4.0f, 0, (selected || hovered) ? 1.5f : 1.0f);

                char orderText[16];
                *std::format_to_n(orderText, sizeof(orderText) - 1, "{}", executionOrder + 1).out = '\0';
                const float textY = std::floor(passMin.y + (passHeight - lineHeight) * 0.5f);
                drawList->AddText(ImVec2(passMin.x + 6.0f, textY), IM_COL32(140, 140, 140, 255), orderText);

                const char* passName = pass->GetName().c_str();
                const ImVec2 nameMin(passMin.x + 6.0f + orderColumnWidth, textY);
                drawList->AddText(nameMin, enabled && !skipped ? IM_COL32(225, 225, 225, 255) : IM_COL32(150, 150, 150, 255), passName);
                if(!enabled)
                {
                    const float strikeY = textY + lineHeight * 0.55f;
                    drawList->AddLine(ImVec2(nameMin.x, strikeY), ImVec2(nameMin.x + ImGui::CalcTextSize(passName).x, strikeY), IM_COL32(200, 70, 70, 255), 1.0f);
                }

                // The swapchain pass presents the frame (and ImGui): disabling it would leave no way to turn it back on
                ImGui::SetCursorScreenPos(checkPos);
                ImGui::BeginDisabled(group.IsSwapchain);
                if(ImGui::Checkbox("##Enabled", &enabled))
                    SetPassEnabled(pass, enabled);
                ImGui::EndDisabled();
                ImGui::SetItemTooltip("%s", group.IsSwapchain ? "Presents the frame, can't be disabled" : (enabled ? "Disable pass" : "Enable pass"));

                ImGui::PopID();

                y += passHeight + PASS_GAP;
                executionOrder++;
            }
        }

        // Edges
        int hoveredEdge = -1;
        ImVec2 points[4];
        if(canvasHovered && !ImGui::IsAnyItemHovered())
        {
            float bestDistSq = EDGE_HOVER_DISTANCE * EDGE_HOVER_DISTANCE;
            for(size_t i = 0; i < mEdges.size(); ++i)
            {
                const int count = edgePoints(mEdges[i], points);
                for(int s = 0; s + 1 < count; ++s)
                {
                    const float distSq = DistToSegmentSq(io.MousePos, points[s], points[s + 1]);
                    if(distSq < bestDistSq)
                    {
                        bestDistSq = distSq;
                        hoveredEdge = static_cast<int>(i);
                    }
                }
            }
        }

        for(size_t i = 0; i < mEdges.size(); ++i)
        {
            const GraphEdge& edge = mEdges[i];

            // Highlight the edges the selected pass produces or consumes
            bool related = false;
            if(selectedPass)
            {
                for(size_t k = edge.FirstImage; k < edge.FirstImage + edge.ImageCount && !related; ++k)
                {
                    const ImageHandle h = mEdgeImages[k].Handle;
                    related = (selectedGroup == edge.Src && Contains(selectedPass->GetImageWrites(), h)) ||
                              (selectedGroup == edge.Dst && Contains(selectedPass->GetImageReads(), h));
                }
            }

            const bool hovered = static_cast<int>(i) == hoveredEdge;
            const ImU32 edgeColor = hovered ? IM_COL32(255, 255, 255, 255) : related ? IM_COL32(255, 175, 45, 255) : IM_COL32(255, 153, 26, 140);
            const float thickness = (hovered || related) ? 2.5f : 1.5f;

            const int count = edgePoints(edge, points);
            const ImVec2 tip = points[count - 1];
            const ImVec2 previous = points[count - 2];
            const float dirX = tip.x > previous.x ? 1.0f : (tip.x < previous.x ? -1.0f : 0.0f);
            const float dirY = tip.y > previous.y ? 1.0f : (tip.y < previous.y ? -1.0f : 0.0f);
            const ImVec2 base(tip.x - dirX * ARROW_LENGTH, tip.y - dirY * ARROW_LENGTH);

            for(int s = 0; s + 1 < count; ++s)
                drawList->AddLine(points[s], (s + 2 == count) ? base : points[s + 1], edgeColor, thickness);
            drawList->AddTriangleFilled(tip, ImVec2(base.x - dirY * ARROW_HALF_WIDTH, base.y + dirX * ARROW_HALF_WIDTH), ImVec2(base.x + dirY * ARROW_HALF_WIDTH, base.y - dirX * ARROW_HALF_WIDTH), edgeColor);

            // Straight edges have room for a label in the gap between groups
            if(edge.Lane < 0)
            {
                char label[128];
                const char* firstName = ImageName(mEdgeImages[edge.FirstImage].Handle);
                const float labelX = points[0].x + 8.0f;
                const float maxLabelWidth = mLayouts[edge.Src].Max.x - labelX;

                if(edge.ImageCount == 1)
                    *std::format_to_n(label, sizeof(label) - 1, "{}", firstName).out = '\0';
                else
                    *std::format_to_n(label, sizeof(label) - 1, "{} +{}", firstName, edge.ImageCount - 1).out = '\0';

                if(ImGui::CalcTextSize(label).x > maxLabelWidth)
                    *std::format_to_n(label, sizeof(label) - 1, "{} image(s)", edge.ImageCount).out = '\0';

                drawList->AddText(ImVec2(labelX, std::floor((points[0].y + points[1].y - lineHeight) * 0.5f)), hovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(200, 200, 200, 200), label);
            }
        }

        if(hoveredEdge >= 0 && ImGui::BeginTooltip())
        {
            const GraphEdge& edge = mEdges[static_cast<size_t>(hoveredEdge)];
            ImGui::Text("%s  ->  %s", groups[edge.Src].Name.c_str(), groups[edge.Dst].Name.c_str());
            ImGui::Separator();
            char usage[160];
            for(size_t k = edge.FirstImage; k < edge.FirstImage + edge.ImageCount; ++k)
                ImGui::BulletText("%s  (-> %s)", ImageName(mEdgeImages[k].Handle), UsageToString(mEdgeImages[k].NewUsage, usage));
            ImGui::EndTooltip();
        }

        // Declare the canvas extents so the child window scrolls correctly
        const float canvasWidth = MARGIN * 2.0f + groupWidth + LANE_GAP * static_cast<float>(laneCount + 1);
        const float canvasHeight = (groupCount > 0 ? mLayouts.back().Max.y - canvasPos.y : 0.0f) + MARGIN;
        ImGui::SetCursorScreenPos(canvasPos);
        ImGui::Dummy(ImVec2(canvasWidth, canvasHeight));
    }

    void RenderGraphVisualizer::DrawPassLinks(RenderGraph& graph, const char* label, ImageHandle image, bool producers, const RenderPass* self)
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        const float maxX = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
        ImGui::TextDisabled("%s", label);

        bool any = false;
        for(const ExecutionGroup& group : graph.GetCompiledGraph().Groups)
        {
            for(RenderPass* pass : group.Passes)
            {
                if(pass == self || !Contains(producers ? pass->GetImageWrites() : pass->GetImageReads(), image))
                    continue;

                const char* name = pass->GetName().c_str();
                const float buttonWidth = ImGui::CalcTextSize(name).x + style.FramePadding.x * 2.0f;
                if(ImGui::GetItemRectMax().x + style.ItemSpacing.x + buttonWidth <= maxX)
                    ImGui::SameLine();

                ImGui::PushID(pass);
                if(ImGui::SmallButton(name))
                    mSelectedPass = pass->GetName();
                ImGui::SetItemTooltip("Inspect %s", name);
                ImGui::PopID();
                any = true;
            }
        }

        if(!any)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("-");
        }
    }

    void RenderGraphVisualizer::DrawImageList(RenderGraph& graph, const char* title, const Vector<ImageHandle>& images, bool isReads, const RenderPass* self)
    {
        char header[64];
        *std::format_to_n(header, sizeof(header) - 1, "{} ({})###{}", title, images.size(), title).out = '\0';
        if(!ImGui::CollapsingHeader(header, ImGuiTreeNodeFlags_DefaultOpen))
            return;

        if(images.empty())
        {
            ImGui::TextDisabled("None");
            return;
        }

        ImFont* boldFont = BoldFont();
        ImGui::PushID(title);
        for(size_t i = 0; i < images.size(); ++i)
        {
            const ImageHandle h = images[i];
            ImGui::PushID(static_cast<int>(i));

            ImGui::PushFont(boldFont, 0.0f);
            ImGui::TextUnformatted(ImageName(h));
            ImGui::PopFont();

            if(mRHI->IsValid(h))
            {
                const ImageDesc& desc = mRHI->GetDesc(h);
                char usage[160];
                ImGui::TextDisabled("%u x %u  |  Mips: %u  |  Layers: %u%s", desc.Width, desc.Height, desc.MipLevel, desc.Layers, desc.Transient ? "  |  Transient" : "");
                ImGui::TextDisabled("Usage: %s", UsageToString(desc.Usage, usage));
                DrawPassLinks(graph, isReads ? "Produced by:" : "Consumed by:", h, isReads, self);

                // ImGui samples it later in this frame's swapchain pass: only images that ended up in the SAMPLED layout can be shown, anything
                // else (never drawn, still an attachment, transient) would be an invalid descriptor read
                const bool previewable = desc.GenerateImGuiID && (desc.Usage & ImageUsage::SAMPLED) && !desc.Transient && desc.Layers == 1 && desc.Width > 0 && desc.Height > 0;
                const bool sampleable = previewable && mRHI->IsImageSampleable(h);
                const ImTextureID textureID = sampleable ? mRHI->GetImGuiImage(h) : ImTextureID{};
                if(textureID)
                {
                    const float maxDim = std::min(mPreviewSize, std::max(ImGui::GetContentRegionAvail().x, 32.0f));
                    ImGui::Image(textureID, FitImage(desc, maxDim));
                    ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(90, 90, 90, 255));
                    if(ImGui::BeginItemTooltip())
                    {
                        ImGui::Image(textureID, FitImage(desc, 512.0f));
                        ImGui::EndTooltip();
                    }
                }
                else if(!previewable)
                    ImGui::TextDisabled("%s", desc.Layers > 1 ? "No preview (layered image)" : "No preview (not visible to ImGui)");
                else
                    ImGui::TextDisabled("No preview this frame (not drawn, or not in a sampled layout)");
            }

            ImGui::Spacing();
            if(i + 1 < images.size())
                ImGui::Separator();
            ImGui::PopID();
        }
        ImGui::PopID();
    }

    void RenderGraphVisualizer::DrawInspector(RenderGraph& graph)
    {
        size_t groupIndex = 0;
        int order = 0;
        RenderPass* pass = FindSelectedPass(graph, groupIndex, order);
        if(!pass)
        {
            ImGui::TextDisabled(mSelectedPass.empty() ? "Select a pass in the graph to inspect it." : "The selected pass isn't part of this graph.");
            return;
        }

        ImGui::PushFont(BoldFont(), 20.0f);
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.1f, 1.0f), "%s", pass->GetName().c_str());
        ImGui::PopFont();
        ImGui::TextDisabled("Group: %s  |  Execution order: #%d", graph.GetCompiledGraph().Groups[groupIndex].Name.c_str(), order + 1);

        const bool swapchain = pass->GetGroup() == PassGroup::SWAPCHAIN;
        bool enabled = pass->IsEnabled();
        ImGui::BeginDisabled(swapchain);
        if(ImGui::Checkbox("Enabled", &enabled))
            SetPassEnabled(pass, enabled);
        ImGui::EndDisabled();
        ImGui::SameLine();
        if(ImGui::SmallButton("Deselect"))
            mSelectedPass.clear();

        ImGui::SetNextItemWidth(std::max(ImGui::GetContentRegionAvail().x * 0.6f, 100.0f));
        ImGui::SliderFloat("Preview Size", &mPreviewSize, 64.0f, 1024.0f, "%.0f px", ImGuiSliderFlags_AlwaysClamp);
        ImGui::Separator();

        DrawImageList(graph, "Reads", pass->GetImageReads(), true, pass);
        DrawImageList(graph, "Writes", pass->GetImageWrites(), false, pass);
    }

} // namespace Surge
