// Copyright (c) - SurgeTechnologies - All rights reserved
#include "RenderGraph.hpp"
#include "Surge/Graphics/RHI/RHI.hpp"
#include <set>
#include "../RHI/Vulkan/VulkanUtils.hpp"

namespace Surge
{
    void RenderGraph::Setup(GraphicsRHI* rhi)
    {
        mRHI = rhi;

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
        ExecutionGroup postProcessGroup = { .Name = "PostProcess", .Type = PassGroup::POST_PROCESS, .Passes = {}, .BarriersBeforeGroup = {}, .Framebuffer {}, .ManagesOwnExecution = false, .IsSwapchain = false };
        ExecutionGroup uiOverlayGroup = { .Name = "UIOverlay", .Type = PassGroup::UI_OVERLAY, .Passes = {}, .BarriersBeforeGroup = {}, .Framebuffer {}, .ManagesOwnExecution = false, .IsSwapchain = false };
        ExecutionGroup swapchainGroup = { .Name = "Swapchain", .Type = PassGroup::SWAPCHAIN, .Passes = {}, .BarriersBeforeGroup = {}, .Framebuffer {}, .IsSwapchain = true };

        for(auto& pass : mPasses)
        {
            switch(pass->GetGroup())
            {
                case PassGroup::SHADOW:       shadowGroup.Passes.push_back(pass.get());      break;
                case PassGroup::MAIN_SCENE:   mainGroup.Passes.push_back(pass.get());        break;
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
        DeriveBarrierBetweenExecutionGroups(mainGroup, postProcessGroup);
        DeriveBarrierBetweenExecutionGroups(outlineGroup, postProcessGroup);
        DeriveBarrierBetweenExecutionGroups(postProcessGroup, uiOverlayGroup);
        DeriveBarrierBetweenExecutionGroups(uiOverlayGroup, swapchainGroup);

        SortByDependencies(shadowGroup.Passes);
        SortByDependencies(mainGroup.Passes);
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
                group.IsSwapchain ? (OnImGuiRender(), mRHI->CmdEndSwapchainRenderpass(ctx)) : mRHI->CmdEndRenderPass(ctx, group.Framebuffer);
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
                            const ImageDesc& writeDesc = mRHI->GetDesc(write);
                            Log<Severity::Warn>("-----------IMAGE BARRIER-----------");
                            Log<Severity::Warn>("Image: {}", writeDesc.DebugName);
                            Log<Severity::Warn>("[After executing {} pass | Before executing {} pass]", writeGroup.Name, readGroup.Name);
                            Log<Severity::Warn>("From: {} -> To: SAMPLED", VulkanUtils::TextureUsageToString(writeDesc.Usage)); //TODO: Remove
                            readGroup.BarriersBeforeGroup.push_back({ write, ImageUsage::SAMPLED });
                            barrierAdded.insert(write);
                        }
                    }
                }
            }
        }
    }

    ///////////
    // ImGui //
    ///////////
    void RenderGraph::OnImGuiRender()
    {
        for(auto& callback : mImGuiRenderCallbacks)
            callback();

        if(!mShowImGui)
            return;

        ImFont* boldFont = ImGui::GetIO().Fonts->Fonts[1];
        if(ImGui::Begin("RenderGraph"))
        {
            if(mCompiledGraph.IsValid && !mCompiledGraph.Groups.empty())
            {
                static int selectedPassIndex = -1;
                if(ImGui::BeginTable("RGTable", 2, ImGuiTableFlags_Resizable))
                {
                    ImGui::TableSetupColumn("Graph", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Inspector", ImGuiTableColumnFlags_WidthStretch);

                    ImGui::TableNextRow();

                    // ========= LEFT COLUMN: Graph =========
                    ImGui::TableSetColumnIndex(0);
                    {
                        ImGui::BeginChild("GraphArea", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

                        ImDrawList* dl = ImGui::GetWindowDrawList();
                        ImVec2 origin = ImGui::GetCursorScreenPos();

                        constexpr float kGroupW = 180.0f;
                        constexpr float kArrowGap = 45.0f;
                        constexpr float kPassH = 24.0f;
                        constexpr float kPassGap = 4.0f;
                        constexpr float kHeaderH = 26.0f;
                        constexpr float kPadY = 8.0f;
                        constexpr float kPadX = 7.0f;
                        constexpr float kRound = 4.0f;
                        constexpr float kSidePad = 0.0f;
                        constexpr float kClearance = 25.0f;
                        constexpr float kBarrierTextH = 18.0f;

                        auto GroupColor = [](PassGroup t) -> ImU32 {
                            switch(t)
                            {
                                case PassGroup::SHADOW:       return IM_COL32(65, 145, 75, 255);
                                case PassGroup::OUTLINE_MASK: return IM_COL32(40, 115, 155, 255);
                                case PassGroup::MAIN_SCENE:   return IM_COL32(120, 75, 120, 255);
                                case PassGroup::POST_PROCESS: return IM_COL32(180, 85, 75, 255);
                                case PassGroup::SWAPCHAIN:    return IM_COL32(175, 140, 45, 255);
                                default:                      return IM_COL32(115, 125, 115, 255);
                            }
                            };

                        const auto& groups = mCompiledGraph.Groups;
                        const size_t numGroups = groups.size();

                        // Dependency edges
                        struct EdgeInfo { size_t srcIdx, dstIdx; Vector<String> imageNames; };
                        Vector<EdgeInfo> edges;
                        for(size_t j = 0; j < numGroups; ++j)
                        {
                            for(const auto& barrier : groups[j].BarriersBeforeGroup)
                            {
                                ImageHandle h = barrier.Handle;
                                size_t srcIdx = SIZE_MAX;
                                for(size_t i = 0; i < numGroups; ++i)
                                {
                                    if(i == j)
                                        continue;

                                    bool writes = false;
                                    for(const auto* pass : groups[i].Passes)
                                        for(ImageHandle w : pass->GetImageWrites())
                                            if(w == h) { writes = true; break; }

                                    if(writes) { srcIdx = i; break; }
                                }
                                if(srcIdx != SIZE_MAX)
                                {
                                    auto it = std::find_if(edges.begin(), edges.end(), [&](const EdgeInfo& e) { return e.srcIdx == srcIdx && e.dstIdx == j; });
                                    if(it == edges.end())
                                        edges.push_back({ srcIdx, j, {mRHI->GetDesc(h).DebugName} });
                                    else
                                        it->imageNames.push_back(mRHI->GetDesc(h).DebugName);
                                }
                            }
                        }

                        // Barrier names per group (footer)
                        Vector<Vector<String>> groupBarrierNames(numGroups);
                        for(const auto& e : edges)
                        {
                            for(const auto& n : e.imageNames)
                                groupBarrierNames[e.srcIdx].push_back(n);
                        }

                        // Group heights
                        Vector<float> groupBaseY(numGroups), groupHeights(numGroups);
                        float yCur = 0.0f;
                        for(size_t gi = 0; gi < numGroups; ++gi)
                        {
                            groupBaseY[gi] = yCur;
                            float passesH = (float)groups[gi].Passes.size() * (kPassH + kPassGap);
                            float barrierH = groupBarrierNames[gi].empty() ? 0.0f
                                : (kPadY + kBarrierTextH + groupBarrierNames[gi].size() * kBarrierTextH);
                            float h = kHeaderH + kPadY + passesH + barrierH + kPadY;
                            groupHeights[gi] = h;
                            yCur += h + kArrowGap;
                        }

                        // X‑offset solving (collision free)
                        const float baseX = origin.x + 10.0f + kSidePad;
                        Vector<float> groupOffsetX(numGroups, 0.0f);
                        bool changed = true;
                        while(changed)
                        {
                            changed = false;
                            for(const auto& e : edges)
                            {
                                if(e.dstIdx <= e.srcIdx + 1)
                                    continue;
                                float srcCenterX = baseX + groupOffsetX[e.srcIdx] + kGroupW * 0.5f;
                                for(size_t k = e.srcIdx + 1; k < e.dstIdx; ++k)
                                {
                                    float neededLeft = srcCenterX + kClearance;
                                    float currentLeft = baseX + groupOffsetX[k];
                                    if(currentLeft < neededLeft)
                                    {
                                        groupOffsetX[k] = neededLeft - baseX;
                                        changed = true;
                                    }
                                }
                            }
                        }

                        // Group rects
                        struct GroupRect { ImVec2 Min, Max; };
                        Vector<GroupRect> groupRects(numGroups);
                        for(size_t gi = 0; gi < numGroups; ++gi)
                        {
                            float x = baseX + groupOffsetX[gi];
                            float y = origin.y + 10.0f + groupBaseY[gi];
                            groupRects[gi] = { ImVec2(x, y), ImVec2(x + kGroupW, y + groupHeights[gi]) };
                        }

                        // Canvas size
                        float maxRight = 0.0f;
                        for(size_t gi = 0; gi < numGroups; ++gi) maxRight = std::max(maxRight, groupRects[gi].Max.x);
                        float totalH = groupBaseY.back() + groupHeights.back() + 20.0f;
                        float totalW = maxRight - origin.x + 30.0f;
                        ImGui::SetCursorPos(ImVec2(0, 0));
                        ImGui::Dummy(ImVec2(totalW, totalH));
                        int passIndex = 0;

                        // Execution groups
                        for(size_t gi = 0; gi < numGroups; ++gi)
                        {
                            const ExecutionGroup& group = groups[gi];
                            ImU32 col = GroupColor(group.Type);
                            const ImVec2& gMin = groupRects[gi].Min;
                            const ImVec2& gMax = groupRects[gi].Max;

                            dl->AddRectFilled(gMin, gMax, IM_COL32(15, 15, 15, 225), kRound);
                            dl->AddRect(gMin, gMax, col, kRound, 0, 2.0f);
                            dl->AddRectFilled(gMin, ImVec2(gMax.x, gMin.y + kHeaderH), col, kRound, ImDrawFlags_RoundCornersTop);

                            const char* gName = group.Name.c_str();
                            ImGui::PushFont(boldFont, 18.0f);
                            ImVec2 nameSz = ImGui::CalcTextSize(gName);
                            dl->AddText(ImVec2(gMin.x + (kGroupW - nameSz.x) * 0.5f, gMin.y + (kHeaderH - nameSz.y) * 0.5f), IM_COL32(5, 5, 5, 255), gName);
                            ImGui::PopFont();

                            float yContent = gMin.y + kHeaderH + kPadY;

                            for(size_t pi = 0; pi < group.Passes.size(); pi++)
                            {
                                RenderPass* pass = group.Passes[pi];
                                bool enabled = pass->IsEnabled();

                                ImVec2 pMin(gMin.x + kPadX, yContent);
                                ImVec2 pMax(gMax.x - kPadX, yContent + kPassH);

                                // Checkbox position
                                float frameHeight = ImGui::GetFrameHeight();
                                float slotOffsetY = (kPassH - frameHeight) * 0.5f;
                                ImVec2 checkboxPos(pMax.x - 16.0f - kPadX, pMin.y + slotOffsetY);

                                // Clickable area of whole pass label
                                ImGui::SetCursorScreenPos(pMin);
                                float clickableWidth = checkboxPos.x - pMin.x - 2.0f;
                                ImGui::InvisibleButton(pass->GetName().c_str(), ImVec2(clickableWidth, kPassH));
                                bool hovered = ImGui::IsItemHovered();
                                if(ImGui::IsItemClicked())
                                    selectedPassIndex = passIndex;

                                bool isSelected = (passIndex == selectedPassIndex);

                                // Draw pass box
                                dl->AddRectFilled(pMin, pMax, isSelected ? IM_COL32(40, 40, 40, 255) : (enabled ? IM_COL32(55, 55, 55, 255) : IM_COL32(200, 60, 60, 255)), 4.0f);
                                dl->AddRect(pMin, pMax, isSelected ? IM_COL32(255, 153, 26, 255) : (hovered ? col : enabled ? IM_COL32(85, 85, 85, 255) : IM_COL32(55, 55, 55, 255)),
                                            4.0f, 0, (hovered || isSelected) ? 1.5f : 0.8f);

                                dl->AddText(ImVec2(pMin.x + 18.0f, pMin.y + (kPassH - ImGui::GetTextLineHeight()) * 0.5f),
                                            enabled ? IM_COL32(215, 215, 215, 255) : IM_COL32(10, 10, 10, 255),
                                            pass->GetName().c_str());

                                // Checkbox
                                ImGui::SetCursorScreenPos(checkboxPos);
                                ImGui::PushID(pass->GetName().c_str());
                                if(ImGui::Checkbox("##Enabled", &enabled))
                                    pass->SetEnabled(enabled);
                                ImGui::PopID();

                                yContent += kPassH + kPassGap;
                                passIndex++;
                            }

                            // Barrier footer
                            if(!groupBarrierNames[gi].empty())
                            {
                                yContent += kPadY;
                                dl->AddLine(ImVec2(gMin.x + kPadX, yContent), ImVec2(gMax.x - kPadX, yContent), IM_COL32(80, 80, 80, 200), 1.0f);
                                yContent += 2.0f;
                                dl->AddText(ImVec2(gMin.x + kPadX, yContent), IM_COL32(255, 195, 70, 255), "BARRIERS");
                                yContent += kBarrierTextH;
                                for(const auto& name : groupBarrierNames[gi])
                                {
                                    dl->AddText(ImVec2(gMin.x + kPadX, yContent), IM_COL32(255, 255, 255, 255), name.c_str());
                                    yContent += kBarrierTextH;
                                }
                            }
                        }

                        // Arrows
                        for(const auto& e : edges)
                        {
                            const ImVec2& srcMin = groupRects[e.srcIdx].Min;
                            const ImVec2& srcMax = groupRects[e.srcIdx].Max;
                            const ImVec2& dstMin = groupRects[e.dstIdx].Min;
                            float srcCX = srcMin.x + kGroupW * 0.5f;
                            float dstCX = dstMin.x + kGroupW * 0.5f;
                            float y1 = srcMax.y, y2 = dstMin.y;
                            dl->AddLine(ImVec2(srcCX, y1), ImVec2(dstCX, y2 - 8.0f), IM_COL32(255, 153, 26, 255), 1.5f);
                            dl->AddTriangleFilled(ImVec2(dstCX, y2), ImVec2(dstCX - 5.0f, y2 - 9.0f), ImVec2(dstCX + 5.0f, y2 - 9.0f), IM_COL32(255, 153, 26, 255));
                        }

                        ImGui::EndChild();
                    }

                    // RIGHT COLUMN: Pass Inspector
                    ImGui::TableSetColumnIndex(1);
                    {
                        ImGui::BeginChild("InspectorArea", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

                        if(selectedPassIndex >= 0)
                        {
                            bool found = false;
                            int idx = 0;
                            for(auto& group : mCompiledGraph.Groups)
                            {
                                for(auto* pass : group.Passes)
                                {
                                    if(idx == selectedPassIndex)
                                    {
                                        ImGui::PushFont(boldFont, 20.0f);
                                        const char* passName = pass->GetName().c_str();
                                        float windowWidth = ImGui::GetWindowSize().x;
                                        float textWidth = ImGui::CalcTextSize(passName).x;
                                        float textPosX = (windowWidth - textWidth) * 0.5f;
                                        ImGui::SetCursorPosX(textPosX);
                                        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.1f, 1.0f), "%s", passName);
                                        ImGui::PopFont();
                                        ImGui::Separator();
                                        static int imageSize = 200.0f;
                                        ImGui::SliderInt("Image Size", &imageSize, 100, 800);
                                        if(ImGui::BeginTable("PassResources", 2, ImGuiTableFlags_NoBordersInBodyUntilResize | ImGuiTableFlags_Resizable))
                                        {
                                            ImGui::TableSetupColumn("READS", ImGuiTableColumnFlags_WidthStretch);
                                            ImGui::TableSetupColumn("WRITES", ImGuiTableColumnFlags_WidthStretch);
                                            ImGui::TableHeadersRow();

                                            ImGui::TableNextRow();
                                            ImGui::TableSetColumnIndex(0);

                                            // Reads
                                            if(!pass->GetImageReads().empty())
                                            {
                                                for(const auto& h : pass->GetImageReads())
                                                {
                                                    const ImageDesc& desc = mRHI->GetDesc(h);
                                                    ImGui::BulletText("%s (%dx%d)", desc.DebugName.c_str(), desc.Width, desc.Height);
                                                    if((desc.Usage & ImageUsage::SAMPLED) && desc.GenerateImGuiID)
                                                    {
                                                        ImTextureID imGuiID = mRHI->GetImGuiImage(h);
                                                        float aspect = (float)desc.Width / (float)desc.Height;
                                                        ImVec2 imgSize = (aspect > 1.0f) ? ImVec2(imageSize, imageSize / aspect) : ImVec2(imageSize * aspect, imageSize);
                                                        ImGui::Image(imGuiID, imgSize);
                                                    }
                                                }
                                            }
                                            else
                                            {
                                                ImGui::PushFont(boldFont, 50.0f);
                                                ImGui::TextDisabled("NONE");
                                                ImGui::PopFont();
                                            }

                                            ImGui::TableSetColumnIndex(1);

                                            // Writes
                                            if(!pass->GetImageWrites().empty())
                                            {
                                                for(const auto& h : pass->GetImageWrites())
                                                {
                                                    const ImageDesc& desc = mRHI->GetDesc(h);
                                                    ImGui::BulletText("%s (%dx%d)", desc.DebugName.c_str(), desc.Width, desc.Height);
                                                    if((desc.Usage & ImageUsage::SAMPLED) && desc.GenerateImGuiID)
                                                    {
                                                        ImTextureID imGuiID = mRHI->GetImGuiImage(h);
                                                        float aspect = (float)desc.Width / (float)desc.Height;
                                                        ImVec2 imgSize = (aspect > 1.0f) ? ImVec2(imageSize, imageSize / aspect) : ImVec2(imageSize * aspect, imageSize);
                                                        ImGui::Image(imGuiID, imgSize);
                                                    }
                                                }
                                            }
                                            else
                                            {
                                                ImGui::PushFont(boldFont, 50.0f);
                                                ImGui::TextDisabled("NONE");
                                                ImGui::PopFont();
                                            }

                                            ImGui::EndTable();
                                        }

                                        found = true;
                                        break;
                                    }
                                    idx++;
                                }
                                if(found)
                                    break;
                            }
                        }
                        else
                            ImGui::TextDisabled("Select a pass in the graph to inspect!");

                        ImGui::EndChild();
                    }
                    ImGui::EndTable();
                }
            }
        }
        ImGui::End(); // RenderGraph

        ImGui::Begin("Renderer");
        for(Scope<RenderPass>& pass : mPasses)
            pass->OnImGuiRender(mBlackboard);
        mRHI->ShowMetricsWindow();
        ImGui::End();
    }
}
