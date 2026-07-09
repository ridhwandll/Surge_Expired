// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Utility/ImGuiAux.hpp"
#include "imgui_internal.h"

namespace Surge
{
    void ImGuiAux::DrawRectAroundWidget(const glm::vec4& color, float thickness, float rounding)
    {
        ImGuiContext& g = *GImGui;
        const ImRect& rect = (g.LastItemData.StatusFlags & ImGuiItemStatusFlags_HasDisplayRect) ? g.LastItemData.DisplayRect : g.LastItemData.Rect;
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRect(rect.Min, rect.Max, ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, color.w)), rounding, ImDrawFlags_RoundCornersAll, thickness);
    }

    void ImGuiAux::TextCentered(const char* text)
    {
        float windowWidth = ImGui::GetWindowSize().x;
        float textWidth = ImGui::CalcTextSize(text).x;
        ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
        ImGui::TextUnformatted(text);
    }

    void ImGuiAux::DockSpace(float titleBarHeight)
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();

        ImVec2 dockspacePos = ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + titleBarHeight);
        ImVec2 dockspaceSize = ImVec2(viewport->WorkSize.x, viewport->WorkSize.y - titleBarHeight);

        ImGui::SetNextWindowPos(dockspacePos);
        ImGui::SetNextWindowSize(dockspaceSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoSavedSettings;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGui::Begin("SurgeMainDockSpace", nullptr, windowFlags);
        ImGui::PopStyleVar(3);

        ImGuiID dockspaceID = ImGui::GetID("SurgeEditorDockspace");
        ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

        ImGui::End();
    }

    bool ImGuiAux::PropertyGridHeader(const String& name, bool openByDefault, const glm::vec2& size, bool spacing)
    {
        ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;

        if (openByDefault)
            treeNodeFlags |= ImGuiTreeNodeFlags_DefaultOpen;

        bool open = false;
        float framePaddingX = size.x;
        float framePaddingY = size.y;

        ImGuiAux::ScopedStyle headerRounding({ImGuiStyleVar_FrameRounding}, 0.0f);
        ImGuiAux::ScopedStyle headerPaddingAndHeight({ImGuiStyleVar_FramePadding}, ImVec2 {framePaddingX, framePaddingY});

        ImGui::PushID(name.c_str());
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]); // Bold font
        String uppercaseName = name;
        for (char& n : uppercaseName)
            n = toupper(n);
        open = ImGui::TreeNodeEx("##dummyId", treeNodeFlags, "%s", uppercaseName.c_str());
        ImGui::PopFont();
        ImGui::PopID();

        DrawRectAroundWidget({0.3f, 0.3f, 0.3f, 1.0f}, 0.2f, 0.1f);
        const float headerSpacingOffset = -(ImGui::GetStyle().ItemSpacing.y + 1.0f);
        if (!spacing)
        {
            if (!open)
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + headerSpacingOffset);
        }

        if(!open)
            ImGui::Dummy({ 0.0f, 5.0f });

        return open;
    }

    bool ImGuiAux::ButtonCentered(const char* title)
    {
        float windowWidth = ImGui::GetWindowSize().x;
        float textWidth = ImGui::CalcTextSize(title).x;
        ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
        bool res = ImGui::Button(title);

        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
            DrawRectAroundWidget(Colors::ThemeColor2, 1.5f, 1.0f);

        return res;
    }

    bool ImGuiAux::Spinner(const char* label, float radius, float thickness)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);

        ImVec2 pos = window->DC.CursorPos;
        ImVec2 size((radius)*2, (radius + style.FramePadding.y) * 2);

        const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
        ImGui::ItemSize(bb, style.FramePadding.y);
        if (!ImGui::ItemAdd(bb, id))
            return false;

        // Render
        window->DrawList->PathClear();

        const int numSegments = 30;
        const int start = static_cast<int>(glm::abs(ImSin(static_cast<float>(g.Time) * 1.8f) * (numSegments - 5)));

        const float aMin = IM_PI * 2.0f * static_cast<float>(start) / static_cast<float>(numSegments);
        const float aMax = IM_PI * 2.0f * (static_cast<float>(numSegments) - 3) / static_cast<float>(numSegments);

        const ImVec2 centre = ImVec2(pos.x + radius, pos.y + radius + style.FramePadding.y);

        for (int i = 0; i < numSegments; i++)
        {
            const float a = aMin + (static_cast<float>(i) / static_cast<float>(numSegments)) * (aMax - aMin);
            window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(a + static_cast<float>(g.Time) * 8) * radius, centre.y + ImSin(a + static_cast<float>(g.Time * 8)) * radius));
        }

        window->DrawList->PathStroke(4293097241, false, thickness);
        return true;
    }


    struct ConfirmationState
    {
        bool IsOpen = false;
        String Title;
        String Message;
        std::function<void()> OnConfirm;
        std::function<void()> OnCancel;
    };
    static ConfirmationState sConfirmState;

    void ImGuiAux::ShowConfirmationBox(const String& title, const String& message, std::function<void()> onConfirm, std::function<void()> onCancel)
    {
        sConfirmState.Title = title;
        sConfirmState.Message = message;
        sConfirmState.OnConfirm = onConfirm;
        sConfirmState.OnCancel = onCancel;
        sConfirmState.IsOpen = true;
    }

    void ImGuiAux::RenderConfirmationBox()
    {
        if(sConfirmState.IsOpen)
        {
            ImGui::OpenPopup(sConfirmState.Title.c_str());
            sConfirmState.IsOpen = false;
        }

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_Appearing);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 16.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));

        bool popupOpen = true;
        ImFont* boldFont = ImGui::GetIO().Fonts->Fonts[1];

        ImGui::PushFont(boldFont);
        ImGui::PushStyleColor(ImGuiCol_Text, ImGuiAux::Colors::Gold);
        if(ImGui::BeginPopupModal(sConfirmState.Title.c_str(), &popupOpen, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::PopFont();
            ImGui::PopStyleColor();

            ImGui::TextWrapped("%s", sConfirmState.Message.c_str());
            ImGui::Dummy(ImVec2(0, 20));

            float spacing = ImGui::GetStyle().ItemSpacing.x;
            float availableWidth = ImGui::GetContentRegionAvail().x;

            // Split the width in half + spacing
            float buttonWidth = (availableWidth - spacing) * 0.5f;

            // CANCEL BUTTON
            ImGui::PushFont(boldFont);
            if(ImGui::Button("CANCEL", ImVec2(buttonWidth, 30)))
            {
                if(sConfirmState.OnCancel)
                    sConfirmState.OnCancel();

                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            // CONFIRM BUTTON
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.15f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.25f, 0.25f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.55f, 0.10f, 0.10f, 1.0f));

            if(ImGui::Button("CONFIRM", ImVec2(buttonWidth, 30)))
            {
                if(sConfirmState.OnConfirm)
                    sConfirmState.OnConfirm();

                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(3);
            ImGui::PopFont();

            ImGui::EndPopup();
        }
        else
        {
            ImGui::PopFont();
            ImGui::PopStyleColor();
        }

        // User clicked the "X"
        if(!popupOpen)
        {
            if(sConfirmState.OnCancel)
                sConfirmState.OnCancel();
        }

        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }

} // namespace Surge