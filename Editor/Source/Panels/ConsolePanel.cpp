// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Panels/ConsolePanel.hpp"
#include "Console/EditorConsole.hpp"
#include "Utility/ImGuiAux.hpp"

#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace Surge
{
    static const char* SeverityTag(Severity severity)
    {
        switch(severity)
        {
            case Severity::Trace: return "TRACE";
            case Severity::Info:  return "INFO";
            case Severity::Debug: return "DEBUG";
            case Severity::Warn:  return "WARN";
            case Severity::Error: return "ERROR";
            case Severity::Fatal: return "FATAL";
        }
        return "";
    }

    static ImVec4 SeverityColor(Severity severity)
    {
        switch(severity)
        {
            case Severity::Trace: return ImVec4(0.6f,  0.6f,  0.6f, 1.0f);
            case Severity::Info:  return ImVec4(0.45f, 0.85f, 0.45f, 1.0f);
            case Severity::Debug: return ImVec4(0.4f,  0.8f,  0.9f, 1.0f);
            case Severity::Warn:  return ImVec4(1.0f,  0.8f,  0.2f, 1.0f);
            case Severity::Error: return ImVec4(1.0f,  0.4f,  0.4f, 1.0f);
            case Severity::Fatal: return ImVec4(1.0f,  0.1f,  0.1f, 1.0f);
        }
        return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    // Colored on/off toggle, returns true when flipped
    static bool FilterToggle(const char* label, bool& value, const ImVec4& color)
    {
        const ImVec4 off = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
        const ImVec4 on = ImVec4(color.x * 0.35f, color.y * 0.35f, color.z * 0.35f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, value ? on : off);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(color.x * 0.5f, color.y * 0.5f, color.z * 0.5f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, value ? color : ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        const bool clicked = ImGui::Button(label);
        ImGui::PopStyleColor(3);
        if(clicked)
            value = !value;
        return clicked;
    }

    void ConsolePanel::Init(void*)
    {
        mCode = GetStaticCode();
    }

    void ConsolePanel::Render(bool* show)
    {
        if(mFocusWindow)
            *show = true;
        if(!*show)
            return;

        const char* windowName = PanelCodeToString(mCode);

        if(mFocusWindow)
        {
            ImGui::SetNextWindowFocus();
            mFocusWindow = false;
        }

        if(ImGui::Begin(windowName, show))
        {
            EditorConsole& console = EditorConsole::Get();
            {
                std::lock_guard<std::mutex> lock(console.GetMutex()); // Nothing in here may call Log()

                if(mFilterDirty || console.GetRevision() != mFilteredRevision)
                    RebuildFilter();

                DrawToolbar();
                DrawMessages();
                DrawDetails();
            }

            if(mClearRequested)
            {
                console.Clear();
                mClearRequested = false;
                mSelectedID = 0;
                mFilterDirty = true;
            }
        }
        ImGui::End();
    }

    void ConsolePanel::Focus()
    {
        ImGui::SetWindowFocus("Console");
        mFocusWindow = true;
    }

    void ConsolePanel::FocusWithSearch(const char* search)
    {
        std::snprintf(mSearch, sizeof(mSearch), "%s", search);
        mFilterDirty = true;
        mScrollToBottom = true;
        Focus();
    }

    void ConsolePanel::DrawToolbar()
    {
        EditorConsole& console = EditorConsole::Get();

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 4.0f));
        {
            ImGuiAux::ScopedBoldFont font;
            if(ImGui::Button("CLEAR"))
                mClearRequested = true; // Applied once the lock is released
        }
        ImGui::SameLine();
        ImGui::Checkbox("Clear on Play", &mClearOnPlay);
        ImGui::SameLine();
        if(ImGui::Checkbox("Auto-scroll", &mAutoScroll) && mAutoScroll)
            mScrollToBottom = true;

        char logsLabel[48], warningsLabel[48], errorsLabel[48];
        std::snprintf(logsLabel, sizeof(logsLabel), "%u LOGS###ConsoleLogs", console.GetCount(Severity::Trace) + console.GetCount(Severity::Info) + console.GetCount(Severity::Debug));
        std::snprintf(warningsLabel, sizeof(warningsLabel), "%u WARNINGS###ConsoleWarnings", console.GetWarningCount());
        std::snprintf(errorsLabel, sizeof(errorsLabel), "%u ERRORS###ConsoleErrors", console.GetErrorCount());

        // Search fills whatever the right aligned toggles leave
        const ImGuiStyle& style = ImGui::GetStyle();
        auto buttonWidth = [&style](const char* label) { return ImGui::CalcTextSize(label, nullptr, true).x + style.FramePadding.x * 2.0f; };
        const float togglesWidth = buttonWidth(logsLabel) + buttonWidth(warningsLabel) + buttonWidth(errorsLabel) + style.ItemSpacing.x * 3.0f;

        ImGui::SameLine();
        ImGui::SetNextItemWidth(std::max(100.0f, ImGui::GetContentRegionAvail().x - togglesWidth));
        if(ImGui::InputTextWithHint("##ConsoleSearch", "Search...", mSearch, sizeof(mSearch)))
            mFilterDirty = true;

        ImGui::SameLine();
        {
            ImGuiAux::ScopedBoldFont font;
            if(FilterToggle(logsLabel, mShowLogs, SeverityColor(Severity::Trace)))
                mFilterDirty = true;
            ImGui::SameLine();
            if(FilterToggle(warningsLabel, mShowWarnings, SeverityColor(Severity::Warn)))
                mFilterDirty = true;
            ImGui::SameLine();
            if(FilterToggle(errorsLabel, mShowErrors, SeverityColor(Severity::Error)))
                mFilterDirty = true;
        }
        ImGui::PopStyleVar();

        if(mFilterDirty)
            RebuildFilter();
    }

    void ConsolePanel::DrawMessages()
    {
        const std::deque<ConsoleMessage>& messages = EditorConsole::Get().GetMessages();
        const bool hasSelection = mSelectedID != 0 && !messages.empty() && mSelectedID >= messages.front().ID && mSelectedID <= messages.back().ID;
        const float detailsHeight = hasSelection ? ImGui::GetTextLineHeightWithSpacing() * 6.0f : 0.0f;

        constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV;
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(6.0f, 2.0f));
        if(ImGui::BeginTable("##ConsoleTable", 4, tableFlags, ImVec2(0.0f, -detailsHeight)))
        {
            ImGui::TableSetupColumn("TIME", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, ImGui::CalcTextSize("00:00:00").x);
            ImGui::TableSetupColumn("LEVEL", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("ERROR").x);
            ImGui::TableSetupColumn("MESSAGE", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("COUNT", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, ImGui::CalcTextSize("x9999").x);

            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(mFiltered.size()));
            while(clipper.Step())
            {
                for(int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
                {
                    const ConsoleMessage& message = messages[mFiltered[row]];
                    const ImVec4 color = SeverityColor(message.Level);
                    const bool emphasize = message.Level >= Severity::Warn;

                    ImGui::PushID(static_cast<int>(message.ID));
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.55f, 1.0f));
                    if(ImGui::Selectable(message.Time, message.ID == mSelectedID, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap))
                        mSelectedID = (message.ID == mSelectedID) ? 0 : message.ID;
                    ImGui::PopStyleColor();

                    ImGui::TableSetColumnIndex(1);
                    {
                        ImGuiAux::ScopedBoldFont font;
                        ImGui::TextColored(color, "%s", SeverityTag(message.Level));
                    }

                    // First line only, the details pane shows the rest
                    ImGui::TableSetColumnIndex(2);
                    const char* textBegin = message.Text.c_str();
                    const char* textEnd = std::strchr(textBegin, '\n');
                    if(emphasize)
                        ImGui::PushStyleColor(ImGuiCol_Text, color);
                    ImGui::TextUnformatted(textBegin, textEnd ? textEnd : textBegin + message.Text.size());
                    if(emphasize)
                        ImGui::PopStyleColor();

                    ImGui::TableSetColumnIndex(3);
                    if(message.Count > 1)
                        ImGui::Text("X%u", message.Count);

                    ImGui::PopID();
                }
            }

            if(mScrollToBottom || (mAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
                ImGui::SetScrollHereY(1.0f);
            mScrollToBottom = false;

            ImGui::EndTable();
        }
        ImGui::PopStyleVar();
    }

    void ConsolePanel::DrawDetails()
    {
        const std::deque<ConsoleMessage>& messages = EditorConsole::Get().GetMessages();
        if(mSelectedID == 0 || messages.empty() || mSelectedID < messages.front().ID || mSelectedID > messages.back().ID)
            return;

        // IDs are consecutive, so the slot is a subtraction away
        const ConsoleMessage& message = messages[static_cast<size_t>(mSelectedID - messages.front().ID)];

        ImGuiAux::StyledSeparator();
        if(ImGui::BeginChild("##ConsoleDetails"))
        {
            {
                ImGuiAux::ScopedBoldFont font;
                ImGui::TextColored(SeverityColor(message.Level), "%s", SeverityTag(message.Level));
            }
            ImGui::SameLine();
            ImGui::TextDisabled("%s", message.Time);
            if(message.Count > 1)
            {
                ImGui::SameLine();
                ImGui::Text("(Logged %u times in a row)", message.Count);
            }
            ImGui::SameLine();
            if(ImGui::SmallButton("Copy"))
                ImGui::SetClipboardText(message.Text.c_str());

            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(message.Text.c_str(), message.Text.c_str() + message.Text.size());
            ImGui::PopTextWrapPos();
        }
        ImGui::EndChild();
    }

    void ConsolePanel::RebuildFilter()
    {
        EditorConsole& console = EditorConsole::Get();
        const std::deque<ConsoleMessage>& messages = console.GetMessages();

        mFiltered.clear();
        mFiltered.reserve(messages.size());
        for(size_t i = 0; i < messages.size(); i++)
        {
            if(PassesFilter(messages[i]))
                mFiltered.push_back(static_cast<Uint>(i));
        }

        mFilteredRevision = console.GetRevision();
        mFilterDirty = false;
    }

    bool ConsolePanel::PassesFilter(const ConsoleMessage& message) const
    {
        switch(message.Level)
        {
            case Severity::Trace:
            case Severity::Info:
            case Severity::Debug:
                if(!mShowLogs)
                    return false;
                break;
            case Severity::Warn:
                if(!mShowWarnings)
                    return false;
                break;
            case Severity::Error:
            case Severity::Fatal:
                if(!mShowErrors)
                    return false;
                break;
        }

        if(mSearch[0] == '\0') // No search
            return true;

        const char* searchEnd = mSearch + std::strlen(mSearch);
        auto it = std::search(message.Text.begin(), message.Text.end(), mSearch, searchEnd,
                              [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)); });

        return it != message.Text.end();
    }

} // namespace Surge
