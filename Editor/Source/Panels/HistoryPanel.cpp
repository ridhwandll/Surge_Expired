// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Panels/HistoryPanel.hpp"
#include "Surge/Core/Core.hpp"
#include "History/EditorHistory.hpp"
#include "Utility/ImGuiAux.hpp"
#include "Editor.hpp"

#include <imgui.h>

namespace Surge
{
    void HistoryPanel::Init(void*)
    {
        mCode = GetStaticCode();
    }

    void HistoryPanel::Render(bool* show)
    {
        if(!*show)
            return;

        if(ImGui::Begin(PanelCodeToString(mCode), show))
        {
            Editor* editor = static_cast<Editor*>(Core::GetClient());
            const EditorHistory& history = editor->GetHistory();
            const bool playing = editor->IsPlaying();

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 4.0f));
            {
                ImGuiAux::ScopedBoldFont font;
                ImGui::BeginDisabled(playing || !history.CanUndo());
                if(ImGui::Button("UNDO"))
                    editor->RequestUndo();
                ImGui::EndDisabled();
                ImGui::SameLine();
                ImGui::BeginDisabled(playing || !history.CanRedo());
                if(ImGui::Button("REDO"))
                    editor->RequestRedo();
                ImGui::EndDisabled();
            }
            ImGui::PopStyleVar();
            ImGui::SameLine();
            ImGui::TextDisabled("%zu / %zu steps", history.GetSize(), EditorHistory::MAX_STEPS);

            if(playing)
                ImGui::TextColored(ImGuiAux::Colors::Red, "Runtime changes are not recorded; UNDO/REDO will NOT work.");

            ImGui::Separator();

            if(ImGui::BeginChild("##HistoryList"))
            {
                const size_t position = history.GetPosition();
                const bool positionChanged = position != mLastPosition || history.GetSize() != mLastSize;

                // Row 0 is the state the scene was opened in, row N is "after command N-1"
                for(size_t row = 0; row <= history.GetSize(); row++)
                {
                    const bool isCurrent = row == position;
                    const bool isRedoable = row > position;
                    const char* label = row == 0 ? "Scene Opened" : history.GetCommand(row - 1).GetName().c_str();

                    ImGui::PushID(static_cast<int>(row));
                    if(isRedoable)
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.45f, 0.45f, 1.0f));
                    if(isCurrent)
                        ImGui::PushStyleColor(ImGuiCol_Text, ImGuiAux::Colors::Green);

                    if(ImGui::Selectable(label, isCurrent, ImGuiSelectableFlags_AllowOverlap) && !isCurrent && !playing)
                        editor->RequestHistoryJump(row);

                    if(isCurrent)
                        ImGui::PopStyleColor();
                    if(isRedoable)
                        ImGui::PopStyleColor();

                    if(history.IsSavedPosition(row))
                    {
                        ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("SAVED").x);
                        ImGuiAux::ScopedBoldFont font;
                        ImGui::TextColored(ImGuiAux::Colors::LightGreen, "SAVED");
                    }

                    if(isCurrent && positionChanged)
                        ImGui::SetScrollHereY(0.5f);
                    ImGui::PopID();
                }

                mLastPosition = position;
                mLastSize = history.GetSize();
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }

} // namespace Surge
