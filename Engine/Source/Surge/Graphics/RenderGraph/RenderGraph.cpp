// Copyright (c) - SurgeTechnologies - All rights reserved
#include "RenderGraph.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Graphics/RHI/RHI.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanUtils.hpp"
#include <set>

namespace Surge
{
    void RenderGraph::Setup(GraphicsRHI* rhi)
    {
        mRHI = rhi;
        mImGuiEnabled = Core::GetClient()->GetClientOptions().EnableImGui;

        // Producers must be registered before consumers so blackboard handles are valid
        for(auto& pass : mPasses)
            pass->Setup(rhi, mBlackboard);
    }

    void RenderGraph::Compile()
    {
        SCOPED_TIMER("RenderGraph::Compile");
        mCompiledGraph = {};

        ExecutionGroup outlineGroup = { .Name = "OutlineMask", .Type = PassGroup::OUTLINE_MASK, .Passes = {}, .BarriersBeforeGroup = {}, .Framebuffer {}, .ManagesOwnExecution = false, .IsSwapchain = false };
        ExecutionGroup shadowGroup = { .Name = "Shadow", .Type = PassGroup::SHADOW, .Passes = {}, .BarriersBeforeGroup = {}, .Framebuffer {}, .ManagesOwnExecution = true, .IsSwapchain = false };
        ExecutionGroup mainGroup = { .Name = "MainScene", .Type = PassGroup::MAIN_SCENE, .Passes = {}, .BarriersBeforeGroup = {}, .Framebuffer {}, .ManagesOwnExecution = false, .IsSwapchain = false };
        ExecutionGroup aoGroup = { .Name = "AmbientOcclusion", .Type = PassGroup::AMBIENT_OCCLUSION, .Passes = {}, .BarriersBeforeGroup = {}, .Framebuffer {}, .ManagesOwnExecution = true, .IsSwapchain = false };
        ExecutionGroup bloomGroup = { .Name = "Bloom", .Type = PassGroup::BLOOM, .Passes = {}, .BarriersBeforeGroup = {}, .Framebuffer {}, .ManagesOwnExecution = true, .IsSwapchain = false };
        ExecutionGroup postProcessGroup = { .Name = "PostProcess", .Type = PassGroup::POST_PROCESS, .Passes = {}, .BarriersBeforeGroup = {}, .Framebuffer {}, .ManagesOwnExecution = false, .IsSwapchain = false };
        ExecutionGroup uiOverlayGroup = { .Name = "UIOverlay", .Type = PassGroup::UI_OVERLAY, .Passes = {}, .BarriersBeforeGroup = {}, .Framebuffer {}, .ManagesOwnExecution = false, .IsSwapchain = false };
        ExecutionGroup swapchainGroup = { .Name = "Swapchain", .Type = PassGroup::SWAPCHAIN, .Passes = {}, .BarriersBeforeGroup = {}, .Framebuffer {}, .IsSwapchain = true };

        for(auto& pass : mPasses)
        {
            switch(pass->GetGroup())
            {
                case PassGroup::SHADOW:       shadowGroup.Passes.push_back(pass.get());      break;
                case PassGroup::MAIN_SCENE:   mainGroup.Passes.push_back(pass.get());        break;
                case PassGroup::AMBIENT_OCCLUSION: aoGroup.Passes.push_back(pass.get());     break;
                case PassGroup::BLOOM:        bloomGroup.Passes.push_back(pass.get());       break;
                case PassGroup::OUTLINE_MASK: outlineGroup.Passes.push_back(pass.get());     break;
                case PassGroup::POST_PROCESS: postProcessGroup.Passes.push_back(pass.get()); break;
                case PassGroup::UI_OVERLAY:   uiOverlayGroup.Passes.push_back(pass.get());   break;
                case PassGroup::SWAPCHAIN:    swapchainGroup.Passes.push_back(pass.get());   break;
            }
        }

        outlineGroup.Framebuffer = mBlackboard.OutlineFramebuffer;
        postProcessGroup.Framebuffer = mBlackboard.PostProcessFramebuffer;
        uiOverlayGroup.Framebuffer = mBlackboard.UIOverlayFramebuffer;
        mainGroup.Framebuffer = mBlackboard.MainPassFramebuffer;

        DeriveBarrierBetweenExecutionGroups(shadowGroup, mainGroup);
        DeriveBarrierBetweenExecutionGroups(mainGroup, aoGroup);
        DeriveBarrierBetweenExecutionGroups(mainGroup, postProcessGroup);
        DeriveBarrierBetweenExecutionGroups(aoGroup, postProcessGroup);
        DeriveBarrierBetweenExecutionGroups(mainGroup, bloomGroup);
        DeriveBarrierBetweenExecutionGroups(bloomGroup, postProcessGroup);
        DeriveBarrierBetweenExecutionGroups(outlineGroup, postProcessGroup);
        DeriveBarrierBetweenExecutionGroups(postProcessGroup, uiOverlayGroup);
        DeriveBarrierBetweenExecutionGroups(uiOverlayGroup, swapchainGroup);

        SortByDependencies(shadowGroup.Passes);
        SortByDependencies(mainGroup.Passes);
        SortByDependencies(aoGroup.Passes);
        SortByDependencies(bloomGroup.Passes);
        SortByDependencies(outlineGroup.Passes);
        SortByDependencies(postProcessGroup.Passes);
        SortByDependencies(uiOverlayGroup.Passes);
        SortByDependencies(swapchainGroup.Passes);

        // Build final groups
        if(!outlineGroup.Passes.empty())
            mCompiledGraph.Groups.push_back(std::move(outlineGroup));

        if(!shadowGroup.Passes.empty())
            mCompiledGraph.Groups.push_back(std::move(shadowGroup));

        mCompiledGraph.Groups.push_back(std::move(mainGroup));

        if(!aoGroup.Passes.empty())
            mCompiledGraph.Groups.push_back(std::move(aoGroup));

        if(!bloomGroup.Passes.empty())
            mCompiledGraph.Groups.push_back(std::move(bloomGroup));

        if (!postProcessGroup.Passes.empty())
            mCompiledGraph.Groups.push_back(std::move(postProcessGroup));

        if(!uiOverlayGroup.Passes.empty())
            mCompiledGraph.Groups.push_back(std::move(uiOverlayGroup));

        if(!swapchainGroup.Passes.empty())
            mCompiledGraph.Groups.push_back(std::move(swapchainGroup));

        mCompiledGraph.IsValid = true;
    }

    void RenderGraph::Execute(FrameContext& ctx)
    {
        for (const ExecutionGroup& group : mCompiledGraph.Groups)
        {
            for(const ImageBarrier& barrier : group.BarriersBeforeGroup)
                mRHI->CmdTransitionImageLayout(ctx, barrier.Handle, barrier.NewUsage);

            if (!group.ManagesOwnExecution)
                group.IsSwapchain ? mRHI->CmdBeginSwapchainRenderpass(ctx) : mRHI->CmdBeginRenderPass(ctx, group.Framebuffer);

            for(RenderPass* pass : group.Passes)
            {
                if (pass->IsEnabled())
                    pass->Execute(ctx, mBlackboard);
            }

            if(!group.ManagesOwnExecution)
                group.IsSwapchain ? (mImGuiEnabled ? OnImGuiRender() : void(), mRHI->CmdEndSwapchainRenderpass(ctx)) : mRHI->CmdEndRenderPass(ctx, group.Framebuffer);
        }
    }

    void RenderGraph::OnWindowResize(Uint width, Uint height)
    {
        if(RHISettings::RENDER_TO_SWAPCHAIN && (width > 0 && height > 0))
        {
            for(auto& node : mPasses)
                node->Resize(width, height, mBlackboard);
        }
    }

    void RenderGraph::ForceResize(Uint width, Uint height)
    {
        for(auto& node : mPasses)
            node->Resize(width, height, mBlackboard);
    }

    void RenderGraph::Shutdown()
    {
        for(auto& pass : mPasses)
            pass->Shutdown(mBlackboard);
    }

    void RenderGraph::AddImGuiRenderCallback(std::function<void()> callback)
    {
        if(callback && mImGuiEnabled)
            mImGuiRenderCallbacks.push_back(std::move(callback));
    }

    void RenderGraph::SortByDependencies(Vector<RenderPass*>& passes)
    {
        //SCOPED_TIMER("RenderGraph::SortByDependencies");

        if(passes.size() <= 1)
            return;

        // Stable bubble-up sort based on direct read/write dependencies
        bool changed = true;
        size_t iterations = 0;
        const size_t maxIterations = passes.size() * passes.size(); // To detect cycles

        while(changed)
        {
            changed = false;
            iterations++;

            if(iterations > maxIterations)
            {
                SG_ASSERT_INTERNAL("Cyclic dependency between passes!");
                return;
            }

            for(size_t i = 0; i < passes.size() - 1; ++i)
            {
                RenderPass* a = passes[i];
                RenderPass* b = passes[i + 1];

                // (Rid)Does pass A depend on pass B?
                // (Meaning B writes to something A reads, so B must go BEFORE A)
                bool aDependsOnB = false;
                for(ImageHandle write : b->GetImageWrites())
                {
                    for(ImageHandle read : a->GetImageReads())
                    {
                        if(write == read)
                        {
                            aDependsOnB = true;
                            break;
                        }
                    }
                    if(aDependsOnB)
                        break;
                }

                if(aDependsOnB)
                {
                    // If A depends on B, swap them so B comes first
                    std::swap(passes[i], passes[i + 1]);
                    changed = true;
                }
            }
        }
    }

    void RenderGraph::DeriveBarrierBetweenExecutionGroups(ExecutionGroup& writeGroup, ExecutionGroup& readGroup)
    {
        // (Rid) Derive barriers between groups
        // These hell for loop basically answers:
        // Does any pass in a group write an image that any pass in b group reads? If so, that image needs a barrier between them
        // Example: MainScene -> Swapchain: FinalImage color -> shader read
        std::set<ImageHandle> barrierAdded;
        for(RenderPass* a : writeGroup.Passes)
        {
            for(RenderPass* b : readGroup.Passes)
            {
                for(ImageHandle write : a->GetImageWrites())
                {
                    for(ImageHandle read : b->GetImageReads())
                    {
                        if(write == read && !barrierAdded.count(write))
                        {
                            readGroup.BarriersBeforeGroup.push_back({ write, ImageUsage::SAMPLED });
                            barrierAdded.insert(write);
#if 0
                            const ImageDesc& writeDesc = mRHI->GetDesc(write);
                            Log<Severity::Warn>("-----------IMAGE BARRIER-----------");
                            Log<Severity::Warn>("Image: {}", writeDesc.DebugName);
                            Log<Severity::Warn>("[After executing {} pass | Before executing {} pass]", writeGroup.Name, readGroup.Name);
                            Log<Severity::Warn>("From: {} -> To: SAMPLED", VulkanUtils::TextureUsageToString(writeDesc.Usage)); //TODO: Remove
#endif
                        }
                    }
                }
            }
        }
    }

    // ImGui (This function is modified by Claude on 7/10/26; 4:09AM with no Human Verification)
    void RenderGraph::OnImGuiRender()
    {
        if(!mImGuiEnabled)
        {
            Log<Severity::Warn>("RenderGraph::OnImGuiRender() called but ImGui is disabled. Enable ImGui in ClientOptions to use Dear ImGui");
            return;
        }

        // Client ImGui callbacks
        for(auto& callback : mImGuiRenderCallbacks)
            callback();

        if(!mShowInternalImGui)
            return;

        struct GroupLayout
        {
            ImVec2 Min, Max;
        };

        struct GraphEdge
        {
            size_t Src, Dst;               // Indices into mCompiledGraph.Groups
            size_t FirstImage, ImageCount; // Range into sEdgeImages
            int Lane;                      // -1 = straight edge between neighbouring groups, otherwise a routing lane right of the groups
            float ExitY, EntryY;
        };

        // (Rid) Editor-only visualizer, layout scratch lives in statics so it doesn't churn the heap every frame
        static Vector<GroupLayout> sLayouts;
        static Vector<GraphEdge> sEdges;
        static Vector<ImageBarrier> sEdgeImages;
        static RenderPass* sSelectedPass = nullptr; // Validated against the compiled graph every frame before use
        static float sPreviewSize = 220.0f;

        constexpr float kMargin = 12.0f;
        constexpr float kGroupMinW = 210.0f;
        constexpr float kGroupGap = 46.0f;
        constexpr float kHeaderH = 28.0f;
        constexpr float kHeaderFontSize = 17.0f;
        constexpr float kPassMinH = 24.0f;
        constexpr float kPassGap = 4.0f;
        constexpr float kPadX = 8.0f;
        constexpr float kPadY = 8.0f;
        constexpr float kRound = 5.0f;
        constexpr float kLaneGap = 18.0f;
        constexpr float kPortInset = 10.0f;
        constexpr float kPortGap = 8.0f;
        constexpr float kArrowLen = 9.0f;
        constexpr float kArrowHalfW = 5.0f;
        constexpr float kEdgeHoverDist = 5.0f;
        constexpr float kGridStep = 24.0f;

        ImGuiIO& io = ImGui::GetIO();
        const ImGuiStyle& style = ImGui::GetStyle();
        ImFont* boldFont = io.Fonts->Fonts.Size > 1 ? io.Fonts->Fonts[1] : nullptr; // nullptr keeps the current font in PushFont()

        const Vector<ExecutionGroup>& groups = mCompiledGraph.Groups;
        const size_t numGroups = groups.size();

        auto Contains = [](const Vector<ImageHandle>& list, ImageHandle h) { return std::find(list.begin(), list.end(), h) != list.end(); };

        auto ImageName = [this](ImageHandle h) -> const char*
        {
            const String& name = mRHI->GetDesc(h).DebugName;
            return name.empty() ? "<Unnamed Image>" : name.c_str();
        };

        auto LocatePass = [&](const RenderPass* target, size_t& outGroup, int& outOrder) -> bool
        {
            int order = 0;
            for(size_t gi = 0; gi < numGroups; ++gi)
            {
                for(const RenderPass* pass : groups[gi].Passes)
                {
                    if(pass == target)
                    {
                        outGroup = gi;
                        outOrder = order;
                        return true;
                    }
                    order++;
                }
            }
            return false;
        };

        auto GroupColor = [](PassGroup type) -> ImU32
        {
            switch(type)
            {
                case PassGroup::SHADOW:            return IM_COL32(65, 145, 75, 255);
                case PassGroup::OUTLINE_MASK:      return IM_COL32(40, 115, 155, 255);
                case PassGroup::MAIN_SCENE:        return IM_COL32(150, 95, 150, 255);
                case PassGroup::AMBIENT_OCCLUSION: return IM_COL32(95, 105, 180, 255);
                case PassGroup::BLOOM:             return IM_COL32(200, 105, 160, 255);
                case PassGroup::POST_PROCESS:      return IM_COL32(190, 95, 80, 255);
                case PassGroup::UI_OVERLAY:        return IM_COL32(60, 150, 150, 255);
                case PassGroup::SWAPCHAIN:         return IM_COL32(175, 140, 45, 255);
                default:                           return IM_COL32(115, 125, 115, 255);
            }
        };

        auto FitImage = [](const ImageDesc& desc, float maxDim) -> ImVec2
        {
            const float aspect = static_cast<float>(desc.Width) / static_cast<float>(desc.Height);
            return aspect >= 1.0f ? ImVec2(maxDim, maxDim / aspect) : ImVec2(maxDim * aspect, maxDim);
        };

        auto DistToSegmentSq = [](const ImVec2& p, const ImVec2& a, const ImVec2& b) -> float
        {
            const float abX = b.x - a.x, abY = b.y - a.y;
            const float apX = p.x - a.x, apY = p.y - a.y;
            const float lenSq = abX * abX + abY * abY;
            float t = lenSq > 0.0f ? (apX * abX + apY * abY) / lenSq : 0.0f;
            t = std::clamp(t, 0.0f, 1.0f);
            const float dx = apX - abX * t, dy = apY - abY * t;
            return dx * dx + dy * dy;
        };

        // Passes can be added/removed between compiles, never keep a selection that isn't part of the current graph
        {
            size_t groupIdx = 0;
            int order = 0;
            if(sSelectedPass && !LocatePass(sSelectedPass, groupIdx, order))
                sSelectedPass = nullptr;
        }

        //////////////////
        // Graph canvas //
        //////////////////
        auto DrawGraph = [&]()
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 canvasPos = ImGui::GetCursorScreenPos(); // Already accounts for scrolling
            const float lineH = ImGui::GetTextLineHeight();
            const float checkSize = ImGui::GetFrameHeight();
            const float passH = std::max(kPassMinH, checkSize + 4.0f);
            const float orderColW = ImGui::CalcTextSize("00").x + 6.0f;

            // Uniform group width that fits the longest label
            float groupW = kGroupMinW;
            ImGui::PushFont(boldFont, kHeaderFontSize);
            for(const ExecutionGroup& group : groups)
                groupW = std::max(groupW, ImGui::CalcTextSize(group.Name.c_str()).x + kPadX * 4.0f);
            ImGui::PopFont();

            for(const ExecutionGroup& group : groups)
            {
                for(const RenderPass* pass : group.Passes)
                    groupW = std::max(groupW, kPadX * 2.0f + 6.0f + orderColW + ImGui::CalcTextSize(pass->GetName().c_str()).x + 8.0f + checkSize + 6.0f);

                for(const ImageBarrier& barrier : group.BarriersBeforeGroup)
                    groupW = std::max(groupW, kPadX * 2.0f + 14.0f + ImGui::CalcTextSize(ImageName(barrier.Handle)).x);
            }

            // Vertical layout in execution order
            sLayouts.resize(numGroups);
            {
                const float x = canvasPos.x + kMargin;
                float y = canvasPos.y + kMargin;
                for(size_t gi = 0; gi < numGroups; ++gi)
                {
                    const ExecutionGroup& group = groups[gi];
                    float h = kHeaderH + kPadY;
                    if(!group.BarriersBeforeGroup.empty())
                        h += lineH + 2.0f + static_cast<float>(group.BarriersBeforeGroup.size()) * lineH + kPadY;

                    const size_t rows = std::max<size_t>(group.Passes.size(), 1);
                    h += static_cast<float>(rows) * passH + static_cast<float>(rows - 1) * kPassGap + kPadY;

                    sLayouts[gi] = { ImVec2(x, y), ImVec2(x + groupW, y + h) };
                    y += h + kGroupGap;
                }
            }

            // Dependency edges: every barrier comes from the most recent group (executed before the reader) that writes the image
            sEdges.clear();
            sEdgeImages.clear();
            auto ProducerGroup = [&](size_t dst, ImageHandle h) -> size_t
            {
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

            for(size_t dst = 0; dst < numGroups; ++dst)
            {
                for(size_t src = dst; src-- > 0;)
                {
                    const size_t first = sEdgeImages.size();
                    for(const ImageBarrier& barrier : groups[dst].BarriersBeforeGroup)
                    {
                        if(ProducerGroup(dst, barrier.Handle) != src)
                            continue;

                        bool duplicate = false;
                        for(size_t k = first; k < sEdgeImages.size() && !duplicate; ++k)
                            duplicate = sEdgeImages[k].Handle == barrier.Handle;

                        if(!duplicate)
                            sEdgeImages.push_back(barrier);
                    }

                    if(sEdgeImages.size() > first)
                        sEdges.push_back({ src, dst, first, sEdgeImages.size() - first, -1, 0.0f, 0.0f });
                }
            }

            // Edges that skip groups are routed through lanes on the right, shortest spans get the innermost lanes
            int laneCount = 0;
            for(size_t span = 2; span < numGroups; ++span)
            {
                for(GraphEdge& e : sEdges)
                {
                    if(e.Dst - e.Src != span)
                        continue;

                    int lane = 0;
                    for(bool taken = true; taken;)
                    {
                        taken = false;
                        for(const GraphEdge& o : sEdges)
                        {
                            if(&o != &e && o.Lane == lane && o.Src < e.Dst && e.Src < o.Dst)
                            {
                                taken = true;
                                lane++;
                                break;
                            }
                        }
                    }
                    e.Lane = lane;
                    laneCount = std::max(laneCount, lane + 1);
                }
            }

            // Ports on the group's right side. Inner lanes leave lowest and enter highest, so lane segments never cross each other
            for(GraphEdge& e : sEdges)
            {
                if(e.Lane < 0)
                    continue;

                int outSlot = 0, inSlot = 0;
                for(const GraphEdge& o : sEdges)
                {
                    if(&o == &e || o.Lane < 0 || o.Lane >= e.Lane)
                        continue;
                    outSlot += (o.Src == e.Src) ? 1 : 0;
                    inSlot += (o.Dst == e.Dst) ? 1 : 0;
                }

                const GroupLayout& src = sLayouts[e.Src];
                const GroupLayout& dst = sLayouts[e.Dst];
                e.ExitY = std::max(src.Max.y - kPortInset - static_cast<float>(outSlot) * kPortGap, src.Min.y + kHeaderH);
                e.EntryY = std::min(dst.Min.y + kHeaderH * 0.5f + static_cast<float>(inSlot) * kPortGap, dst.Max.y - kPortInset);
            }

            auto EdgePoints = [&](const GraphEdge& e, ImVec2* pts) -> int
            {
                const GroupLayout& src = sLayouts[e.Src];
                const GroupLayout& dst = sLayouts[e.Dst];
                if(e.Lane < 0)
                {
                    const float cx = (src.Min.x + src.Max.x) * 0.5f;
                    pts[0] = ImVec2(cx, src.Max.y);
                    pts[1] = ImVec2(cx, dst.Min.y);
                    return 2;
                }

                const float laneX = src.Max.x + kLaneGap * static_cast<float>(e.Lane + 1);
                pts[0] = ImVec2(src.Max.x, e.ExitY);
                pts[1] = ImVec2(laneX, e.ExitY);
                pts[2] = ImVec2(laneX, e.EntryY);
                pts[3] = ImVec2(dst.Max.x, e.EntryY);
                return 4;
            };

            const bool canvasHovered = ImGui::IsWindowHovered();

            // Background grid (visible region only)
            {
                const ImVec2 winMin = ImGui::GetWindowPos();
                const ImVec2 winMax(winMin.x + ImGui::GetWindowSize().x, winMin.y + ImGui::GetWindowSize().y);
                const ImU32 gridCol = IM_COL32(255, 255, 255, 10);
                const float startX = canvasPos.x + static_cast<float>(static_cast<int>((winMin.x - canvasPos.x) / kGridStep) - 1) * kGridStep;
                const float startY = canvasPos.y + static_cast<float>(static_cast<int>((winMin.y - canvasPos.y) / kGridStep) - 1) * kGridStep;
                for(float gx = startX; gx < winMax.x; gx += kGridStep)
                    dl->AddLine(ImVec2(gx, winMin.y), ImVec2(gx, winMax.y), gridCol);
                for(float gy = startY; gy < winMax.y; gy += kGridStep)
                    dl->AddLine(ImVec2(winMin.x, gy), ImVec2(winMax.x, gy), gridCol);
            }

            // Execution groups
            size_t selectedGroup = SIZE_MAX;
            {
                int unusedOrder = 0;
                if(sSelectedPass)
                    LocatePass(sSelectedPass, selectedGroup, unusedOrder);
            }

            int executionOrder = 0;
            for(size_t gi = 0; gi < numGroups; ++gi)
            {
                const ExecutionGroup& group = groups[gi];
                const GroupLayout& gl = sLayouts[gi];
                const ImU32 col = GroupColor(group.Type);
                const ImVec2 headerMax(gl.Max.x, gl.Min.y + kHeaderH);

                dl->AddRectFilled(gl.Min, gl.Max, IM_COL32(18, 18, 18, 235), kRound);
                dl->AddRectFilled(gl.Min, headerMax, col, kRound, ImDrawFlags_RoundCornersTop);
                dl->AddRect(gl.Min, gl.Max, col, kRound, 0, gi == selectedGroup ? 2.5f : 1.5f);

                ImGui::PushFont(boldFont, kHeaderFontSize);
                const ImVec2 nameSize = ImGui::CalcTextSize(group.Name.c_str());
                dl->AddText(ImVec2(gl.Min.x + (groupW - nameSize.x) * 0.5f, gl.Min.y + (kHeaderH - nameSize.y) * 0.5f), IM_COL32(10, 10, 10, 255), group.Name.c_str());
                ImGui::PopFont();

                if(canvasHovered && ImGui::IsMouseHoveringRect(gl.Min, headerMax) && ImGui::BeginTooltip())
                {
                    ImGui::TextUnformatted(group.Name.c_str());
                    ImGui::Separator();
                    ImGui::Text("Passes: %u", static_cast<Uint>(group.Passes.size()));
                    ImGui::Text("Barriers before group: %u", static_cast<Uint>(group.BarriersBeforeGroup.size()));
                    ImGui::Text("Render pass: %s", group.IsSwapchain ? "Swapchain" : (group.ManagesOwnExecution ? "Managed by its passes" : "Begun by RenderGraph"));
                    ImGui::EndTooltip();
                }

                float y = gl.Min.y + kHeaderH + kPadY;

                // Barriers recorded before this group begins
                if(!group.BarriersBeforeGroup.empty())
                {
                    dl->AddText(ImVec2(gl.Min.x + kPadX, y), IM_COL32(255, 195, 70, 255), "BARRIERS");
                    y += lineH + 2.0f;
                    for(const ImageBarrier& barrier : group.BarriersBeforeGroup)
                    {
                        dl->AddCircleFilled(ImVec2(gl.Min.x + kPadX + 4.0f, y + lineH * 0.5f), 2.5f, IM_COL32(255, 195, 70, 255));
                        dl->AddText(ImVec2(gl.Min.x + kPadX + 14.0f, y), IM_COL32(220, 220, 220, 255), ImageName(barrier.Handle));
                        y += lineH;
                    }
                    dl->AddLine(ImVec2(gl.Min.x + kPadX, y + kPadY * 0.5f), ImVec2(gl.Max.x - kPadX, y + kPadY * 0.5f), IM_COL32(80, 80, 80, 200));
                    y += kPadY;
                }

                if(group.Passes.empty())
                {
                    const char* emptyText = "(No passes)";
                    const ImVec2 textSize = ImGui::CalcTextSize(emptyText);
                    dl->AddText(ImVec2(gl.Min.x + (groupW - textSize.x) * 0.5f, y + (passH - textSize.y) * 0.5f), ImGui::GetColorU32(ImGuiCol_TextDisabled), emptyText);
                }

                for(RenderPass* pass : group.Passes)
                {
                    bool enabled = pass->IsEnabled();
                    const ImVec2 pMin(gl.Min.x + kPadX, y);
                    const ImVec2 pMax(gl.Max.x - kPadX, y + passH);
                    const ImVec2 checkPos(pMax.x - checkSize - 3.0f, pMin.y + (passH - checkSize) * 0.5f);

                    ImGui::PushID(pass);

                    // Whole row (minus the checkbox) selects the pass
                    ImGui::SetCursorScreenPos(pMin);
                    if(ImGui::InvisibleButton("##Pass", ImVec2(std::max(checkPos.x - pMin.x - 2.0f, 1.0f), passH)))
                    {
                        sSelectedPass = pass;
                        selectedGroup = gi;
                    }
                    const bool hovered = ImGui::IsItemHovered();
                    if(ImGui::BeginItemTooltip())
                    {
                        ImGui::TextUnformatted(pass->GetName().c_str());
                        ImGui::TextDisabled("Reads: %u  |  Writes: %u", static_cast<Uint>(pass->GetImageReads().size()), static_cast<Uint>(pass->GetImageWrites().size()));
                        ImGui::TextDisabled("Click to inspect");
                        ImGui::EndTooltip();
                    }

                    const bool selected = pass == sSelectedPass;
                    const ImU32 fillCol = selected ? IM_COL32(72, 52, 28, 255) : hovered ? IM_COL32(64, 64, 64, 255) : enabled ? IM_COL32(48, 48, 48, 255) : IM_COL32(62, 28, 28, 255);
                    const ImU32 borderCol = selected ? IM_COL32(255, 153, 26, 255) : hovered ? col : enabled ? IM_COL32(80, 80, 80, 255) : IM_COL32(150, 55, 55, 255);
                    dl->AddRectFilled(pMin, pMax, fillCol, 4.0f);
                    dl->AddRect(pMin, pMax, borderCol, 4.0f, 0, (selected || hovered) ? 1.5f : 1.0f);

                    char orderText[16];
                    *std::format_to_n(orderText, sizeof(orderText) - 1, "{}", executionOrder + 1).out = '\0';
                    const float textY = pMin.y + (passH - lineH) * 0.5f;
                    dl->AddText(ImVec2(pMin.x + 6.0f, textY), IM_COL32(140, 140, 140, 255), orderText);

                    const char* passName = pass->GetName().c_str();
                    const ImVec2 nameMin(pMin.x + 6.0f + orderColW, textY);
                    dl->AddText(nameMin, enabled ? IM_COL32(225, 225, 225, 255) : IM_COL32(150, 150, 150, 255), passName);
                    if(!enabled)
                    {
                        const float strikeY = textY + lineH * 0.55f;
                        dl->AddLine(ImVec2(nameMin.x, strikeY), ImVec2(nameMin.x + ImGui::CalcTextSize(passName).x, strikeY), IM_COL32(200, 70, 70, 255), 1.0f);
                    }

                    ImGui::SetCursorScreenPos(checkPos);
                    if(ImGui::Checkbox("##Enabled", &enabled))
                        pass->SetEnabled(enabled);
                    ImGui::SetItemTooltip("%s", enabled ? "Disable pass" : "Enable pass");

                    ImGui::PopID();

                    y += passH + kPassGap;
                    executionOrder++;
                }
            }

            // Edges
            int hoveredEdge = -1;
            ImVec2 pts[4];
            if(canvasHovered && !ImGui::IsAnyItemHovered())
            {
                float bestDistSq = kEdgeHoverDist * kEdgeHoverDist;
                for(size_t i = 0; i < sEdges.size(); ++i)
                {
                    const int count = EdgePoints(sEdges[i], pts);
                    for(int s = 0; s + 1 < count; ++s)
                    {
                        const float distSq = DistToSegmentSq(io.MousePos, pts[s], pts[s + 1]);
                        if(distSq < bestDistSq)
                        {
                            bestDistSq = distSq;
                            hoveredEdge = static_cast<int>(i);
                        }
                    }
                }
            }

            for(size_t i = 0; i < sEdges.size(); ++i)
            {
                const GraphEdge& e = sEdges[i];

                // Highlight the edges the selected pass produces or consumes
                bool related = false;
                if(sSelectedPass)
                {
                    for(size_t k = e.FirstImage; k < e.FirstImage + e.ImageCount && !related; ++k)
                    {
                        const ImageHandle h = sEdgeImages[k].Handle;
                        related = (selectedGroup == e.Src && Contains(sSelectedPass->GetImageWrites(), h)) ||
                                  (selectedGroup == e.Dst && Contains(sSelectedPass->GetImageReads(), h));
                    }
                }

                const bool hovered = static_cast<int>(i) == hoveredEdge;
                const ImU32 edgeCol = hovered ? IM_COL32(255, 255, 255, 255) : related ? IM_COL32(255, 175, 45, 255) : IM_COL32(255, 153, 26, 140);
                const float thickness = (hovered || related) ? 2.5f : 1.5f;

                const int count = EdgePoints(e, pts);
                const ImVec2 tip = pts[count - 1];
                const ImVec2 prev = pts[count - 2];
                const float dirX = tip.x > prev.x ? 1.0f : (tip.x < prev.x ? -1.0f : 0.0f);
                const float dirY = tip.y > prev.y ? 1.0f : (tip.y < prev.y ? -1.0f : 0.0f);
                const ImVec2 base(tip.x - dirX * kArrowLen, tip.y - dirY * kArrowLen);

                for(int s = 0; s + 1 < count; ++s)
                    dl->AddLine(pts[s], (s + 2 == count) ? base : pts[s + 1], edgeCol, thickness);
                dl->AddTriangleFilled(tip, ImVec2(base.x - dirY * kArrowHalfW, base.y + dirX * kArrowHalfW), ImVec2(base.x + dirY * kArrowHalfW, base.y - dirX * kArrowHalfW), edgeCol);

                // Straight edges have room for a label in the gap between groups
                if(e.Lane < 0)
                {
                    char label[128];
                    const char* firstName = ImageName(sEdgeImages[e.FirstImage].Handle);
                    const float labelX = pts[0].x + 8.0f;
                    const float maxLabelW = sLayouts[e.Src].Max.x - labelX;

                    if(e.ImageCount == 1)
                        *std::format_to_n(label, sizeof(label) - 1, "{}", firstName).out = '\0';
                    else
                        *std::format_to_n(label, sizeof(label) - 1, "{} +{}", firstName, e.ImageCount - 1).out = '\0';

                    if(ImGui::CalcTextSize(label).x > maxLabelW)
                        *std::format_to_n(label, sizeof(label) - 1, "{} image(s)", e.ImageCount).out = '\0';

                    dl->AddText(ImVec2(labelX, (pts[0].y + pts[1].y - lineH) * 0.5f), hovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(200, 200, 200, 200), label);
                }
            }

            if(hoveredEdge >= 0 && ImGui::BeginTooltip())
            {
                const GraphEdge& e = sEdges[static_cast<size_t>(hoveredEdge)];
                ImGui::Text("%s  ->  %s", groups[e.Src].Name.c_str(), groups[e.Dst].Name.c_str());
                ImGui::Separator();
                for(size_t k = e.FirstImage; k < e.FirstImage + e.ImageCount; ++k)
                    ImGui::BulletText("%s  (-> %s)", ImageName(sEdgeImages[k].Handle), VulkanUtils::TextureUsageToString(sEdgeImages[k].NewUsage));
                ImGui::EndTooltip();
            }

            // Declare the canvas extents so the child window scrolls correctly
            const float canvasW = kMargin * 2.0f + groupW + kLaneGap * static_cast<float>(laneCount + 1);
            const float canvasH = (numGroups > 0 ? sLayouts.back().Max.y - canvasPos.y : 0.0f) + kMargin;
            ImGui::SetCursorScreenPos(canvasPos);
            ImGui::Dummy(ImVec2(canvasW, canvasH));
        };

        ////////////////////
        // Pass inspector //
        ////////////////////
        auto PassLinks = [&](const char* label, ImageHandle h, bool producers, const RenderPass* self)
        {
            const float maxX = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
            ImGui::TextDisabled("%s", label);

            bool any = false;
            for(const ExecutionGroup& group : groups)
            {
                for(RenderPass* pass : group.Passes)
                {
                    if(pass == self || !Contains(producers ? pass->GetImageWrites() : pass->GetImageReads(), h))
                        continue;

                    const char* name = pass->GetName().c_str();
                    const float buttonW = ImGui::CalcTextSize(name).x + style.FramePadding.x * 2.0f;
                    if(ImGui::GetItemRectMax().x + style.ItemSpacing.x + buttonW <= maxX)
                        ImGui::SameLine();

                    ImGui::PushID(pass);
                    if(ImGui::SmallButton(name))
                        sSelectedPass = pass;
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
        };

        auto DrawImageList = [&](const char* title, const Vector<ImageHandle>& images, bool isReads, const RenderPass* self)
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

            ImGui::PushID(title);
            for(size_t i = 0; i < images.size(); ++i)
            {
                const ImageHandle h = images[i];
                const ImageDesc& desc = mRHI->GetDesc(h);
                ImGui::PushID(static_cast<int>(i));

                ImGui::PushFont(boldFont, 0.0f);
                ImGui::TextUnformatted(ImageName(h));
                ImGui::PopFont();
                ImGui::TextDisabled("%u x %u  |  Mips: %u  |  Layers: %u%s", desc.Width, desc.Height, desc.MipLevel, desc.Layers, desc.Transient ? "  |  Transient" : "");
                ImGui::TextDisabled("Usage: %s", VulkanUtils::TextureUsageToString(desc.Usage));
                PassLinks(isReads ? "Produced by:" : "Consumed by:", h, isReads, self);

                // Layered images (e.g. shadow cascades) can't be sampled by ImGui's 2D shader
                const bool canPreview = desc.GenerateImGuiID && (desc.Usage & ImageUsage::SAMPLED) && desc.Layers == 1 && desc.Width > 0 && desc.Height > 0;
                const ImTextureID textureID = canPreview ? mRHI->GetImGuiImage(h) : ImTextureID{};
                if(textureID)
                {
                    const float maxDim = std::min(sPreviewSize, std::max(ImGui::GetContentRegionAvail().x, 32.0f));
                    ImGui::Image(textureID, FitImage(desc, maxDim));
                    ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(90, 90, 90, 255));
                    if(ImGui::BeginItemTooltip())
                    {
                        ImGui::Image(textureID, FitImage(desc, 512.0f));
                        ImGui::EndTooltip();
                    }
                }
                else
                {
                    ImGui::TextDisabled("%s", desc.Layers > 1 ? "No preview (layered image)" : "No preview (not ImGui visible)");
                }

                ImGui::Spacing();
                if(i + 1 < images.size())
                    ImGui::Separator();
                ImGui::PopID();
            }
            ImGui::PopID();
        };

        auto DrawInspector = [&]()
        {
            RenderPass* pass = sSelectedPass;
            size_t groupIdx = 0;
            int order = 0;
            if(!pass || !LocatePass(pass, groupIdx, order))
            {
                ImGui::TextDisabled("Select a pass in the graph to inspect it.");
                return;
            }

            ImGui::PushFont(boldFont, 20.0f);
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.1f, 1.0f), "%s", pass->GetName().c_str());
            ImGui::PopFont();
            ImGui::TextDisabled("Group: %s  |  Execution order: #%d", groups[groupIdx].Name.c_str(), order + 1);

            bool enabled = pass->IsEnabled();
            if(ImGui::Checkbox("Enabled", &enabled))
                pass->SetEnabled(enabled);
            ImGui::SameLine();
            if(ImGui::SmallButton("Deselect"))
                sSelectedPass = nullptr;

            ImGui::SetNextItemWidth(std::max(ImGui::GetContentRegionAvail().x * 0.6f, 100.0f));
            ImGui::SliderFloat("Preview Size", &sPreviewSize, 64.0f, 1024.0f, "%.0f px", ImGuiSliderFlags_AlwaysClamp);
            ImGui::Separator();

            DrawImageList("Reads", pass->GetImageReads(), true, pass);
            DrawImageList("Writes", pass->GetImageWrites(), false, pass);
        };

        ImGui::SetNextWindowSize(ImVec2(960.0f, 640.0f), ImGuiCond_FirstUseEver);
        if(ImGui::Begin("RenderGraph"))
        {
            if(!mRHI || !mCompiledGraph.IsValid || groups.empty())
            {
                ImGui::TextDisabled("RenderGraph has not been compiled yet.");
            }
            else
            {
                // Toolbar
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

                ImGui::Text("Groups: %u  |  Passes: %u (%u enabled)  |  Barriers: %u", static_cast<Uint>(numGroups), passCount, enabledCount, barrierCount);

                const char* enableAllLabel = "Enable All Passes";
                const float buttonW = ImGui::CalcTextSize(enableAllLabel).x + style.FramePadding.x * 2.0f;
                ImGui::SameLine();
                const float avail = ImGui::GetContentRegionAvail().x;
                if(avail > buttonW)
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - buttonW);

                ImGui::BeginDisabled(enabledCount == passCount);
                if(ImGui::Button(enableAllLabel))
                {
                    for(const ExecutionGroup& group : groups)
                        for(RenderPass* pass : group.Passes)
                            pass->SetEnabled(true);
                }
                ImGui::EndDisabled();
                ImGui::Separator();

                if(ImGui::BeginTable("##RenderGraphLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV, ImGui::GetContentRegionAvail()))
                {
                    ImGui::TableSetupColumn("Graph", ImGuiTableColumnFlags_WidthStretch, 0.6f);
                    ImGui::TableSetupColumn("Inspector", ImGuiTableColumnFlags_WidthStretch, 0.4f);
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    if(ImGui::BeginChild("##GraphCanvas", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
                        DrawGraph();
                    ImGui::EndChild();

                    ImGui::TableSetColumnIndex(1);
                    if(ImGui::BeginChild("##PassInspector", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
                        DrawInspector();
                    ImGui::EndChild();

                    ImGui::EndTable();
                }
            }
        }
        ImGui::End(); // RenderGraph

        ImGui::Begin("Renderer");
        for(Scope<RenderPass>& pass : mPasses)
        {
            ImGui::PushID(pass.get()); // Passes share this window, keep their widget IDs apart
            pass->OnImGuiRender(mBlackboard);
            ImGui::PopID();
        }
        if(mRHI)
            mRHI->ShowMetricsWindow();
        ImGui::End();
    }
}
