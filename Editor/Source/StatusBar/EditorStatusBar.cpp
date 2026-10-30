// Copyright (c) - SurgeTechnologies - All rights reserved
#include "StatusBar/EditorStatusBar.hpp"
#include "Console/EditorConsole.hpp"
#include "Panels/ConsolePanel.hpp"
#include "Utility/EditorUI.hpp"
#include "Surge/Core/Core.hpp"
#include "Editor.hpp"

#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace Surge
{
    namespace
    {
        constexpr auto TASK_SHOW_DELAY = std::chrono::milliseconds(200); // Tasks that finish quicker never flash on screen
        constexpr float SIDE_PADDING = 14.0f;

        float SecondsSince(std::chrono::steady_clock::time_point time, std::chrono::steady_clock::time_point now)
        {
            return std::chrono::duration<float>(now - time).count();
        }

        // Wrapped: log lines (compiler errors, paths) can be very long
        constexpr ImGuiHoveredFlags TOOLTIP_FLAGS = ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_AllowWhenBlockedByPopup;
    } // namespace

    EditorStatusBar& EditorStatusBar::Get()
    {
        static EditorStatusBar sInstance;
        return sInstance;
    }

    void EditorStatusBar::Notify(Severity severity, const String& text, float seconds)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mNotification.Level = severity;
        mNotification.Text = text;
        mNotification.Seconds = seconds;
        mNotification.Time = std::chrono::steady_clock::now();
    }

    void EditorStatusBar::SetTask(const String& key, const String& title, float progress, const String& detail)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        auto it = std::find_if(mTasks.begin(), mTasks.end(), [&key](const Task& task) { return task.Key == key; });
        if(it == mTasks.end())
        {
            Task& task = mTasks.emplace_back();
            task.Key = key;
            task.StartTime = std::chrono::steady_clock::now();
            it = mTasks.end() - 1;
        }

        it->Title = title;
        it->Detail = detail;
        it->Progress = progress < 0.0f ? INDETERMINATE : std::min(progress, 1.0f);
    }

    void EditorStatusBar::EndTask(const String& key)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        std::erase_if(mTasks, [&key](const Task& task) { return task.Key == key; });
    }

    void EditorStatusBar::SyncLatestLog()
    {
        EditorConsole& console = EditorConsole::Get();
        std::lock_guard<std::mutex> lock(console.GetMutex());
        if(console.GetRevision() == mConsoleRevision)
            return;

        mConsoleRevision = console.GetRevision();
        const std::deque<ConsoleMessage>& messages = console.GetMessages();
        mHasLog = !messages.empty(); // Cleared console: the footer clears too
        if(!mHasLog)
            return;

        const ConsoleMessage& latest = messages.back();
        mLogLevel = latest.Level;
        mLogText = latest.Text;
        mLogCount = latest.Count;
        std::memcpy(mLogTime, latest.Time, sizeof(mLogTime));
    }

    void EditorStatusBar::Render()
    {
        SyncLatestLog();

        const TimePoint now = std::chrono::steady_clock::now();
        const float easing = std::min(1.0f, ImGui::GetIO().DeltaTime * 8.0f);
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mDrawTasks.clear();
            for(Task& task : mTasks)
            {
                if(task.Progress >= 0.0f)
                    task.DisplayProgress += (task.Progress - task.DisplayProgress) * easing;
                if(now - task.StartTime >= TASK_SHOW_DELAY)
                    mDrawTasks.push_back(task);
            }
            mDrawNotification = mNotification;
        }

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + viewport->WorkSize.y - HEIGHT));
        ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, HEIGHT));
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGui::PushStyleColor(ImGuiCol_WindowBg, EditorUI::Colors::Background);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                           ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
        if(ImGui::Begin(WINDOW_NAME, nullptr, flags))
        {
            const ImVec2 windowPos = ImGui::GetWindowPos();
            const float width = ImGui::GetWindowWidth();
            ImGui::GetWindowDrawList()->AddLine(windowPos, ImVec2(windowPos.x + width, windowPos.y), ImGui::GetColorU32(EditorUI::Colors::Border));

            const float tasksWidth = mDrawTasks.empty() ? 0.0f : DrawTasks(width - SIDE_PADDING, ROW_HEIGHT);
            const float gap = tasksWidth > 0.0f ? 24.0f : 0.0f;
            DrawMessage(SIDE_PADDING, width - SIDE_PADDING * 2.0f - tasksWidth - gap, ROW_HEIGHT);
        }
        ImGui::End();

        ImGui::PopStyleVar(4);
        ImGui::PopStyleColor();
    }

    void EditorStatusBar::DrawMessage(float x, float width, float height)
    {
        if(width < 24.0f)
            return;

        const float notificationAge = SecondsSince(mDrawNotification.Time, std::chrono::steady_clock::now());
        const bool notifying = !mDrawNotification.Text.empty() && notificationAge < mDrawNotification.Seconds;

        Severity level = Severity::Trace;
        const char* text = "Ready";
        const char* time = nullptr;
        float alpha = 1.0f;
        if(notifying)
        {
            level = mDrawNotification.Level;
            text = mDrawNotification.Text.c_str();
            alpha = std::clamp(mDrawNotification.Seconds - notificationAge, 0.0f, 1.0f); // Fades out during the last second
        }
        else if(mHasLog)
        {
            level = mLogLevel;
            text = mLogText.c_str();
            time = mLogTime;
        }

        // First line only, the tooltip has the rest
        const char* firstLineEnd = std::strchr(text, '\n');
        if(!firstLineEnd)
            firstLineEnd = text + std::strlen(text);

        ImGui::SetCursorPos(ImVec2(x, 0.0f));
        ImGui::InvisibleButton("##StatusMessage", ImVec2(width, height));
        const bool hovered = ImGui::IsItemHovered();
        if(hovered && (notifying || mHasLog))
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if(ImGui::IsItemClicked())
            static_cast<Editor*>(Core::GetClient())->GetPanelManager().GetPanel<ConsolePanel>()->Focus();

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        const float textY = min.y + (height - ImGui::GetFontSize()) * 0.5f;
        if(hovered)
            drawList->AddRectFilled(min, max, ImGui::GetColorU32(EditorUI::WithAlpha(EditorUI::Colors::Ink, 0.04f)));

        ImVec4 color = EditorUI::SeverityColor(level);
        color.w *= alpha;
        float cursorX = min.x + 4.0f;
        if(notifying || mHasLog)
        {
            drawList->AddCircleFilled(ImVec2(cursorX, min.y + height * 0.5f), 3.5f, ImGui::GetColorU32(color));
            cursorX += 12.0f;
        }

        if(time)
        {
            drawList->AddText(ImVec2(cursorX, textY), ImGui::GetColorU32(EditorUI::WithAlpha(EditorUI::Colors::Tone45, alpha)), time);
            cursorX += ImGui::CalcTextSize(time).x + 8.0f;
        }

        // Plain logs in the regular text color, warnings/errors and notifications in their severity color
        const bool tinted = notifying || level >= Severity::Warn;
        ImVec4 textColor = (notifying || mHasLog) ? EditorUI::WithAlpha(EditorUI::Colors::Tone82, alpha) : EditorUI::Colors::Tone45;
        if(tinted)
            textColor = color;

        float textEndX = max.x - 4.0f;
        char countText[16] = {};
        if(!notifying && mHasLog && mLogCount > 1)
        {
            std::snprintf(countText, sizeof(countText), "x%u", mLogCount);
            textEndX -= ImGui::CalcTextSize(countText).x + 10.0f;
        }

        if(textEndX > cursorX)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, textColor);
            ImGui::RenderTextEllipsis(drawList, ImVec2(cursorX, textY), ImVec2(textEndX, max.y), textEndX, text, firstLineEnd, nullptr);
            ImGui::PopStyleColor();

            if(countText[0])
            {
                const float textWidth = std::min(ImGui::CalcTextSize(text, firstLineEnd).x, textEndX - cursorX);
                drawList->AddText(ImVec2(cursorX + textWidth + 8.0f, textY), ImGui::GetColorU32(EditorUI::Colors::Hint), countText);
            }
        }

        if((notifying || mHasLog) && EditorUI::BeginStyledTooltip(TOOLTIP_FLAGS, ImGui::GetFontSize() * 40.0f))
        {
            ImGui::TextUnformatted(text);
            ImGui::TextDisabled("Click to open the Console");
            EditorUI::EndStyledTooltip();
        }
    }

    float EditorStatusBar::DrawTasks(float rightX, float height)
    {
        constexpr float barWidth = 140.0f;
        constexpr float barHeight = 6.0f;
        constexpr float spacing = 8.0f;
        constexpr float maxTitleWidth = 300.0f;

        const Task& primary = mDrawTasks.back(); // Newest
        const size_t others = mDrawTasks.size() - 1;

        char percentText[8] = {};
        if(primary.Progress >= 0.0f)
            std::snprintf(percentText, sizeof(percentText), "%d%%", static_cast<int>(primary.DisplayProgress * 100.0f + 0.5f));
        char othersText[16] = {};
        if(others > 0)
            std::snprintf(othersText, sizeof(othersText), "+%zu", others);

        const float percentWidth = ImGui::CalcTextSize("100%").x;
        const float othersWidth = others > 0 ? ImGui::CalcTextSize(othersText).x + spacing : 0.0f;
        const float titleWidth = std::min(ImGui::CalcTextSize(primary.Title.c_str()).x, maxTitleWidth);
        const float totalWidth = titleWidth + othersWidth + spacing + barWidth + spacing + percentWidth;
        const float startX = rightX - totalWidth;

        ImGui::SetCursorPos(ImVec2(startX, 0.0f));
        ImGui::InvisibleButton("##StatusTasks", ImVec2(totalWidth, height));

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 min = ImGui::GetItemRectMin();
        const float textY = min.y + (height - ImGui::GetFontSize()) * 0.5f;
        float cursorX = min.x;

        ImGui::PushStyleColor(ImGuiCol_Text, EditorUI::Colors::Tone82);
        ImGui::RenderTextEllipsis(drawList, ImVec2(cursorX, textY), ImVec2(cursorX + titleWidth, min.y + height), cursorX + titleWidth, primary.Title.c_str(), nullptr, nullptr);
        ImGui::PopStyleColor();
        cursorX += titleWidth + spacing;

        if(others > 0)
        {
            drawList->AddText(ImVec2(cursorX, textY), ImGui::GetColorU32(EditorUI::Colors::Accent), othersText);
            cursorX += othersWidth;
        }

        const float barY = min.y + (height - barHeight) * 0.5f;
        EditorUI::AddProgressBar(drawList, ImVec2(cursorX, barY), ImVec2(cursorX + barWidth, barY + barHeight), primary.Progress >= 0.0f ? primary.DisplayProgress : INDETERMINATE,
                                 EditorUI::Colors::Selection, EditorUI::Colors::Tone18);
        cursorX += barWidth + spacing;

        if(percentText[0])
        {
            const float width = ImGui::CalcTextSize(percentText).x;
            drawList->AddText(ImVec2(cursorX + percentWidth - width, textY), ImGui::GetColorU32(EditorUI::Colors::Tone60), percentText);
        }

        if(EditorUI::BeginStyledTooltip(TOOLTIP_FLAGS, ImGui::GetFontSize() * 40.0f))
        {
            for(auto it = mDrawTasks.rbegin(); it != mDrawTasks.rend(); ++it)
            {
                {
                    EditorUI::ScopedBoldFont bold;
                    ImGui::TextUnformatted(it->Title.c_str());
                }
                if(!it->Detail.empty())
                    ImGui::TextDisabled("%s", it->Detail.c_str());

                const ImVec2 barMin = ImGui::GetCursorScreenPos();
                const float tooltipBarWidth = ImGui::GetFontSize() * 16.0f;
                ImGui::Dummy(ImVec2(tooltipBarWidth, barHeight));
                EditorUI::AddProgressBar(ImGui::GetWindowDrawList(), barMin, ImVec2(barMin.x + tooltipBarWidth, barMin.y + barHeight), it->Progress >= 0.0f ? it->DisplayProgress : INDETERMINATE,
                                         EditorUI::Colors::Selection, EditorUI::Colors::Tone18);
                if(it + 1 != mDrawTasks.rend())
                    ImGui::Separator();
            }
            EditorUI::EndStyledTooltip();
        }

        return totalWidth;
    }

} // namespace Surge
