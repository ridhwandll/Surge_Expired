// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Panels/ExportPanel.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Utility/FileDialogs.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Utility/ImGuiAux.hpp"
#include "Editor.hpp"
#include <imgui.h>

#undef CopyDirectory // FUCK ASS Windows.h 

namespace Surge
{
    static int sActiveExportTab = 0; // 0 = Windows, 1 = Android
    static String sWindowsOutputPath;
    static String sAndroidKeystorePath;
    static String sAndroidOutputPath;

    void ExportPanel::Init(void* panelInitArgs)
    {
        mCode = GetStaticCode();
    }

    void ExportPanel::OnEvent(Event& e)
    {
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<KeyPressedEvent>([&](KeyPressedEvent& keyEvent) {});
    }

    void ExportPanel::Render(bool* show)
    {
        if(!*show)
            return;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(25.0f, 25.0f));

        if(ImGui::Begin("Export Project", show))
        {
            ImFont* regularFont = ImGui::GetIO().Fonts->Fonts[0];
            ImFont* boldFont = ImGui::GetIO().Fonts->Fonts[1];

            constexpr ImVec4 accentColor = ImGuiAux::Colors::ThemeColor1;
            constexpr ImVec4 accentHover = ImGuiAux::Colors::ThemeColor2;
            constexpr ImVec4 inputBg = ImVec4(0.14f, 0.14f, 0.14f, 1.0f);
            constexpr ImVec4 inputHovered = ImVec4(0.16f, 0.16f, 0.16f, 1.0f);
            constexpr ImVec4 textMuted = ImVec4(0.60f, 0.60f, 0.65f, 1.0f);
            constexpr ImVec4 textActive = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
            constexpr ImVec4 trackBg = ImVec4(0.06f, 0.06f, 0.06f, 1.0f);
            constexpr float rounding = 3.0f;

            float availWidth = ImGui::GetContentRegionAvail().x;

            ImGui::PushFont(boldFont);
            float tabWidth = (availWidth - 8.0f) * 0.5f; // 8px gap
            float tabHeight = 45.0f;

            ImVec2 trackPos = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImVec2(trackPos.x - 4.0f, trackPos.y - 4.0f),
                ImVec2(trackPos.x + availWidth + 4.0f, trackPos.y + tabHeight + 4.0f),
                ImGui::ColorConvertFloat4ToU32(trackBg),
                rounding
            );

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 0.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, rounding);

            // Tab 1: Windows
            ImGui::PushStyleColor(ImGuiCol_Button, sActiveExportTab == 0 ? accentColor : ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, sActiveExportTab == 0 ? accentHover : inputBg);
            ImGui::PushStyleColor(ImGuiCol_Text, sActiveExportTab == 0 ? ImVec4(0, 0, 0, 1) : textMuted);
            if(ImGui::Button("Windows", ImVec2(tabWidth, tabHeight))) sActiveExportTab = 0;
            ImGui::PopStyleColor(3);

            ImGui::SameLine();

            // Tab 2: Android
            ImGui::PushStyleColor(ImGuiCol_Button, sActiveExportTab == 1 ? accentColor : ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, sActiveExportTab == 1 ? accentHover : inputBg);
            ImGui::PushStyleColor(ImGuiCol_Text, sActiveExportTab == 1 ? ImVec4(0, 0, 0, 1) : textMuted);
            if(ImGui::Button("Android", ImVec2(tabWidth, tabHeight))) sActiveExportTab = 1;
            ImGui::PopStyleColor(3);

            ImGui::PopStyleVar(2);
            ImGui::PopFont();

            ImGui::Dummy(ImVec2(0.0f, 25.0f));

            // EXPORT SETTINGS CONTENT
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14.0f, 10.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, rounding);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, inputBg);
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, inputHovered);

            if(sActiveExportTab == 0)
            {
                // Basic Settings
                ImGui::PushFont(boldFont);
                ImGui::TextColored(textActive, "BUILD DESTINATION");
                ImGui::PopFont();
                ImGui::Dummy(ImVec2(0.0f, 2.0f));

                float browseBtnWidth = ImGui::CalcTextSize(" BROWSE ").x + 28.0f; // 28 is padding
                ImGui::SetNextItemWidth(availWidth - browseBtnWidth - ImGui::GetStyle().ItemSpacing.x);

                char winPathBuffer[512];
                strncpy_s(winPathBuffer, sWindowsOutputPath.c_str(), sizeof(winPathBuffer));

                ImGui::PushFont(regularFont);
                ImGui::InputText("##WinOut", winPathBuffer, sizeof(winPathBuffer), ImGuiInputTextFlags_ReadOnly);
                ImGui::PopFont();

                ImGui::SameLine();
                ImGui::PushFont(boldFont);
                ImGui::PushStyleColor(ImGuiCol_Button, inputBg);
                if(ImGuiAux::Button(" BROWSE ", ImVec2(browseBtnWidth, 0.0f)))
                {
                    String selectedPath = FileDialog::ChooseFolder();
                    if(!selectedPath.empty())
                        sWindowsOutputPath = selectedPath;
                }
                ImGui::PopStyleColor();
                ImGui::PopFont();

                ImGui::Dummy(ImVec2(0.0f, 15.0f));

                // Advanced Options
                ImGui::PushFont(boldFont);
                if(ImGui::CollapsingHeader("Advanced Settings", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::PopFont();

                    ImGui::PushFont(regularFont);
                    ImGui::Dummy(ImVec2(0.0f, 5.0f));
                    ImGui::TextColored(textMuted, "Target Architecture:");
                    ImGui::SameLine(180.0f);
                    ImGui::SetNextItemWidth(availWidth - 180.0f);
                    if(ImGui::BeginCombo("##WinArch", "64-bit")) { ImGui::EndCombo(); }
                    ImGui::PopFont();
                }
                else
                    ImGui::PopFont();

                // Main Export Button
                ImGui::Dummy(ImVec2(0.0f, 20.0f));
                bool canExport = !sWindowsOutputPath.empty();
                if(!canExport)
                    ImGui::BeginDisabled();

                ImGui::PushFont(boldFont, 22.0f);
                ImGui::PushStyleColor(ImGuiCol_Button, accentColor);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, accentHover);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
                if(ImGuiAux::Button("BUILD FOR WINDOWS", ImVec2(availWidth, 55.0f)))
                    BuildWindows();

                ImGui::PopStyleColor(3);
                ImGui::PopFont();

                if(!canExport)
                    ImGui::EndDisabled();
            }
            else if(sActiveExportTab == 1)
            {
                float browseBtnWidth = ImGui::CalcTextSize(" BROWSE ").x + 28.0f;

                // Keystore
                ImGui::PushFont(boldFont);
                ImGui::TextColored(textActive, "SIGNING KEYSTORE");
                ImGui::PopFont();
                ImGui::Dummy(ImVec2(0.0f, 2.0f));

                ImGui::SetNextItemWidth(availWidth - browseBtnWidth - ImGui::GetStyle().ItemSpacing.x);
                char keyPathBuffer[512];
                strncpy_s(keyPathBuffer, sAndroidKeystorePath.c_str(), sizeof(keyPathBuffer));

                ImGui::PushFont(regularFont);
                ImGui::InputText("##AndKey", keyPathBuffer, sizeof(keyPathBuffer), ImGuiInputTextFlags_ReadOnly);
                ImGui::PopFont();

                ImGui::SameLine();
                ImGui::PushFont(boldFont);
                ImGui::PushStyleColor(ImGuiCol_Button, inputBg);
                if(ImGuiAux::Button(" BROWSE ", ImVec2(browseBtnWidth, 0.0f)))
                {
                    String selectedPath = FileDialog::OpenFile("Android Keystore (.keystore)\0*.keystore\0All Files (*.*)\0*.*\0");
                    if(!selectedPath.empty())
                        sAndroidKeystorePath = selectedPath;
                }
                ImGui::PopStyleColor();
                ImGui::PopFont();

                ImGui::Dummy(ImVec2(0.0f, 15.0f));

                // Output Destination
                ImGui::PushFont(boldFont);
                ImGui::TextColored(textActive, "BUILD DESTINATION");
                ImGui::PopFont();
                ImGui::Dummy(ImVec2(0.0f, 2.0f));

                ImGui::SetNextItemWidth(availWidth - browseBtnWidth - ImGui::GetStyle().ItemSpacing.x);
                char apkPathBuffer[512];
                strncpy_s(apkPathBuffer, sAndroidOutputPath.c_str(), sizeof(apkPathBuffer));

                ImGui::PushFont(regularFont);
                ImGui::InputText("##AndOut", apkPathBuffer, sizeof(apkPathBuffer), ImGuiInputTextFlags_ReadOnly);
                ImGui::PopFont();

                ImGui::SameLine();
                ImGui::PushFont(boldFont);
                ImGui::PushStyleColor(ImGuiCol_Button, inputBg);
                if(ImGuiAux::Button(" BROWSE ##ApkBrowse", ImVec2(browseBtnWidth, 0.0f)))
                {
                    String selectedPath = FileDialog::SaveFile("Android Package (.apk)\0*.apk\0All Files (*.*)\0*.*\0");
                    if(!selectedPath.empty())
                        sAndroidOutputPath = selectedPath;
                }
                ImGui::PopStyleColor();
                ImGui::PopFont();

                ImGui::Dummy(ImVec2(0.0f, 15.0f));

                // Expandable Advanced Options
                ImGui::PushFont(boldFont);
                if(ImGui::CollapsingHeader("Advanced Settings", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::PopFont();
                    ImGui::PushFont(regularFont);

                    ImGui::Dummy(ImVec2(0.0f, 5.0f));
                    ImGui::TextColored(textMuted, "Target Architecture:");
                    ImGui::SameLine(180.0f);
                    ImGui::SetNextItemWidth(availWidth - 180.0f);
                    if(ImGui::BeginCombo("##AndArch", "ARM64-v8a")) { ImGui::EndCombo(); }

                    ImGui::TextColored(textMuted, "Minimum API Level:");
                    ImGui::SameLine(180.0f);
                    ImGui::SetNextItemWidth(availWidth - 180.0f);
                    if(ImGui::BeginCombo("##AndAPI", "API Level 30 (Android 11)")) { ImGui::EndCombo(); }

                    ImGui::PopFont();
                }
                else
                    ImGui::PopFont();

                // Main Export Button
                ImGui::Dummy(ImVec2(0.0f, 20.0f));
                bool canExport = !sAndroidOutputPath.empty() && !sAndroidKeystorePath.empty();
                if(!canExport)
                    ImGui::BeginDisabled();

                ImGui::PushFont(boldFont, 22.0f);
                ImGui::PushStyleColor(ImGuiCol_Button, accentColor);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, accentHover);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
                if(ImGuiAux::Button("BUILD FOR ANDROID", ImVec2(availWidth, 55.0f)))
                    BuildAndroid();
                ImGui::PopStyleColor(3);
                ImGui::PopFont();

                if(!canExport)
                    ImGui::EndDisabled();
            }

            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(2);
        }
        ImGui::End();

        ImGui::PopStyleVar();
    }
     
    void ExportPanel::Shutdown()
    {
    }

    void ExportPanel::BuildWindows()
    {
        Filesystem::CreateOrEnsureDirectories(sWindowsOutputPath);

        // Engine Files
        Filesystem::CopyFile("build/Player/Release/Player.exe", sWindowsOutputPath + "/Player.exe");
        Filesystem::CopyFile("build/Player/Release/shaderc_shared.dll", sWindowsOutputPath + "/shaderc_shared.dll");

        Path fontsPath = sWindowsOutputPath + "/Engine/Assets/Fonts";
        Filesystem::CreateOrEnsureDirectories(fontsPath);
        Filesystem::CopyDirectory("Engine/Assets/Fonts", fontsPath);

        Path shadersPath = sWindowsOutputPath + "/Engine/Assets/Shaders";
        Filesystem::CreateOrEnsureDirectories(shadersPath);
        Filesystem::CopyDirectory("Engine/Assets/Shaders", shadersPath);

        AssetManager* am = Core::GetAssetManager();
        Editor* editor = (Editor*)Core::GetClient();

        const AssetImporter& assetImporter = editor->GetAssetImporter();
        assetImporter.ScanAndCook();

        // Project Files
        Path assetsPath = am->GetAssetsDirectory();
        Filesystem::CopyDirectory(assetsPath.parent_path(), sWindowsOutputPath + "/Engine");
    }

    void ExportPanel::BuildAndroid()
    {

    }

} // namespace Surge