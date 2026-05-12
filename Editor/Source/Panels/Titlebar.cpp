// Copyright (c) - SurgeTechnologies - All rights reserved
#define IMGUI_DEFINE_MATH_OPERATORS
#include "Panels/Titlebar.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Utility/Platform.hpp"
#include "Surge/Utility/FileDialogs.hpp"
#include "Surge/Serializer/Serializer.hpp"
#include "Utility/ImGUIAux.hpp"
#include "Editor.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <IconsFontAwesome.hpp>

namespace Surge
{
    void Titlebar::OnInit()
    {
        //mIcon = Texture2D::Create("Engine/Assets/Textures/Surge.png");
        mEditor = static_cast<Editor*>(Core::GetClient());
    }

    void Titlebar::Render()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.0f, 8.0f));
        Editor* editor = static_cast<Editor*>(Core::GetClient());

        // Start drawing window
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowBgAlpha(1.0f);
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y));
        ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, GetHeight()));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.12f, 0.12f, 0.12f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_Button, {0.12f, 0.12f, 0.12f, 1.0f});
        if (ImGui::Begin("##dummy", NULL, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoDecoration))
        {
            constexpr float iconSize = 20.0f;
            //ImGuiAux::Image(mIcon->GetImage2D(), {iconSize * 2, iconSize * 2});
            ImGuiAux::ShiftCursorY(iconSize);
            ImGui::SameLine();

            // Only show the File, View and Play button when there is an active project
            if (editor->GetActiveProject())
            {
                if (ImGui::SmallButton("File"))
                    ImGui::OpenPopup("FilePopup");
                if (ImGui::BeginPopup("FilePopup"))
                {
                    if (ImGui::MenuItem("Save Project"))
                    {
                        Log("Implement Project save here!");
                    }
                    ImGui::EndPopup();
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("View"))
                    ImGui::OpenPopup("ViewPopup");
                if (ImGui::BeginPopup("ViewPopup"))
                {
                    PanelManager& panelManager = editor->GetPanelManager();

                    for (auto& [code, element] : panelManager.GetAllPanels())
                    {
                        if (ImGui::MenuItem(PanelCodeToString(code)))
                            element.Show = !element.Show;
                    }

                    ImGui::EndPopup();
                }

                ImGuiAux::ShiftCursorY(-iconSize);
//                 ProjectState projectState = mEditor->GetActiveProject().GetState();
                 float windowWidth = ImGui::GetWindowSize().x;
//                 float textWidth = projectState == ProjectState::Edit ? ImGui::CalcTextSize(ICON_SURGE_PLAY ICON_SURGE_CODE ICON_SURGE_CHECK).x : ImGui::CalcTextSize(ICON_SURGE_PLAY).x;
//                 ImGui::SetCursorPosX((windowWidth - (textWidth + 10)) * 0.5f);
//                 if (projectState == ProjectState::Edit)
//                 {
//                     if (ImGuiAux::Button(ICON_SURGE_PLAY))
//                     {
//                         mEditor->OnRuntimeStart();
//                     }
//                 }
//                 else if (projectState == ProjectState::Play)
//                 {
//                     if (ImGuiAux::Button(ICON_SURGE_STOP))
//                     {
//                         mEditor->OnRuntimeEnd();
//                     }
//                 }

                ImGui::SameLine();
                {
                    String activeProjectName = "Dummy Project";
                    ImGui::SetCursorPosX(windowWidth - 200);
                    ImGui::Text(activeProjectName.c_str());

                    ImGuiContext& g = *GImGui;
                    ImRect& rect = (g.LastItemData.StatusFlags & ImGuiItemStatusFlags_HasDisplayRect) ? g.LastItemData.DisplayRect : g.LastItemData.Rect;
                    rect.Expand(5);     // Expand the rect by 5 units
                    rect.TranslateY(2); // Translate 2 units down Y axis to "Hide the bottom line"
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    //drawList->AddRect(rect.Min, rect.Max, ImGui::ColorConvertFloat4ToU32(ImGuiAux::Colors::ThemeColorLight), 0.8f, 1.0f);

                    const char* label = "Projects";
                    const ImGuiID id = ImGui::GetCurrentWindow()->GetID(label);
                    bool hovered = false;
                    if (ImGui::ButtonBehavior(rect, id, &hovered, nullptr, 0))
                    {
                        Window* window = Core::GetWindow();
                        window->RestoreFromMaximize();
                    }
                    ImGuiAux::DelayedToolTip("Click for opening ProjectBrowser");
					//if (hovered)
					//    drawList->AddRect(rect.Min, rect.Max, ImGui::ColorConvertFloat4ToU32(ImGuiAux::Colors::Silver), 0.8f, ImDrawCornerFlags_All, 1.0f);
                }
            }

            // System buttons
            {
                Window* window = Core::GetWindow();
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                const float buttonSize = ImGui::GetFrameHeight();
                const float iconMargin = buttonSize * 0.33f;
                const float buttonOffsetFromWindowBorder = 10.0f;
                const ImVec2 windowPos = ImGui::GetWindowPos();
                const ImVec2 windowSize = ImGui::GetWindowSize();
                ImRect buttonRect {};
                buttonRect.Min = windowPos + ImVec2(ImGui::GetWindowWidth() - buttonSize, buttonOffsetFromWindowBorder);
                buttonRect.Max = windowPos + windowSize - ImVec2(0, 20.0f);
                buttonRect.TranslateX(-buttonOffsetFromWindowBorder);

                // Exit button
                {
                    bool hovered = false;
                    bool held = false;
                    bool pressed = ImGui::ButtonBehavior(buttonRect, ImGui::GetID("EXIT"), &hovered, &held);

                    if (hovered)
                    {
                        const ImU32 color = ImGui::GetColorU32(ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
                        drawList->AddRectFilled(buttonRect.Min, buttonRect.Max, color, 1.0f);
                    }

                    // Render the cross
                    {
                        const ImU32 color = ImGui::GetColorU32({1.0f, 1.0f, 1.0f, 1.0f});
                        drawList->AddLine(buttonRect.Min + ImVec2(iconMargin, iconMargin), buttonRect.Max - ImVec2(iconMargin, iconMargin), color, 1.5f);
                        drawList->AddLine(buttonRect.Min + ImVec2(buttonRect.GetWidth() - iconMargin, iconMargin), buttonRect.Max - ImVec2(buttonRect.GetWidth() - iconMargin, iconMargin), color, 1.5f);
                    }

                    if (pressed)
                        Platform::RequestExit();

                    buttonRect.Min.x -= buttonSize;
                    buttonRect.Max.x -= buttonSize;
                }

                // Maximize button
                {
                    bool hovered = false;
                    bool held = false;
                    bool pressed = ImGui::ButtonBehavior(buttonRect, ImGui::GetID("MAXIMIZE"), &hovered, &held, 0);

                    if (hovered)
                    {
                        const ImU32 color = ImGui::GetColorU32(held ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered);
                        drawList->AddRectFilled(buttonRect.Min, buttonRect.Max, color, 1.0f);
                    }

                    // Render the box
                    {
                        const ImU32 color = ImGui::GetColorU32({1.0f, 1.0f, 1.0f, 1.0f});
                        drawList->AddRect(buttonRect.Min + ImVec2(iconMargin, iconMargin), buttonRect.Max - ImVec2(iconMargin, iconMargin), color, 0.0f, 0, 1.2f);
                    }

                    if (pressed)
                    {
                        if (!window->IsWindowMaximized())
                            window->Maximize();
                        else
                            window->RestoreFromMaximize();
                    }

                    buttonRect.Min.x -= buttonSize;
                    buttonRect.Max.x -= buttonSize;
                }

                // Minimize button
                {
                    bool hovered = false;
                    bool held = false;
                    bool pressed = ImGui::ButtonBehavior(buttonRect, ImGui::GetID("MINIMIZE"), &hovered, &held, 0);

                    if (hovered)
                    {
                        const ImU32 color = ImGui::GetColorU32(held ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered);
                        drawList->AddRectFilled(buttonRect.Min, buttonRect.Max, color, 1.0f);
                    }

                    // Render the Line
                    {
                        const ImU32 color = ImGui::GetColorU32(ImGuiCol_Text);
                        drawList->AddLine(buttonRect.Min + ImVec2(iconMargin, buttonRect.GetHeight() / 2), buttonRect.Max - ImVec2(iconMargin, buttonRect.GetHeight() / 2), color, 1.2f);
                    }

                    if (pressed)
                        window->Minimize();

                    buttonRect.Min.x -= buttonSize;
                    buttonRect.Max.x -= buttonSize;
                }
            }
        }
        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
} // namespace Surge