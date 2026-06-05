// Copyright (c) - SurgeTechnologies - All rights reserved
#include "ProjectBrowser.hpp"

#include "Surge/Core/Core.hpp"
#include "Surge/Utility/FileDialogs.hpp"
#include "Surge/Core/Memory.hpp"
#include "Surge/ECS/Scene.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Serializer/Serializer.hpp"
#include "Surge/Utility/Filesystem.hpp"

#include "Utility/ImGuiAux.hpp"
#include "Panels/SceneHierarchyPanel.hpp"
#include "Editor.hpp"

#include <imgui.h>
#include <Panels/ContentBrowserPanel.hpp>


namespace Surge
{
    struct RecentProject
    {
        String Name;
        String Filepath;
        String LastOpened;
    };
    // TODO: Populate this list from a config file that stores recent projects
    static Vector<RecentProject> sRecentProjects = {};

    static Project sTempProjectBuffer;
    static String sOpenProjectPath;
    static String sCreateProjectPath;
    static int sActiveTab = 0; // 0 = CREATE PROJECT, 1 = OPEN PROJECT

    void ProjectBrowser::Render()
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImVec4 windowBg = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, windowBg);

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus;

        if(ImGui::Begin("Surge Engine Launcher", nullptr, windowFlags))
        {
            ImFont* regularFont = ImGui::GetIO().Fonts->Fonts[0];
            ImFont* boldFont = ImGui::GetIO().Fonts->Fonts[1];

            constexpr ImVec4 accentColor = ImGuiAux::Colors::ThemeColor1;
            constexpr ImVec4 accentHover = ImGuiAux::Colors::ThemeColor2;
            constexpr ImVec4 accentActive = ImGuiAux::Colors::ThemeColor1;

            constexpr ImVec4 cardBg       = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
            constexpr ImVec4 inputBg      = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
            constexpr ImVec4 inputHovered = ImVec4(0.15f, 0.15f, 0.16f, 1.0f);
            constexpr ImVec4 trackBg      = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
            constexpr ImVec4 listBg       = ImVec4(0.11f, 0.11f, 0.11f, 1.0f);
            constexpr ImVec4 textMuted    = ImVec4(0.50f, 0.50f, 0.50f, 1.0f);
            constexpr ImVec4 textActive   = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

            // Selectable items
            constexpr ImVec4 selectedBlack = ImVec4(0.02f, 0.02f, 0.02f, 1.0f);
            constexpr ImVec4 hoverGray     = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);
            constexpr ImVec4 activeGray    = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);

            constexpr float rounding = 7.0f;
            const float formWidth = ImGui::GetContentRegionAvail().x;
            const float formHeight = ImGui::GetContentRegionAvail().y;

            ImVec2 availSize = ImGui::GetContentRegionAvail();
            float offsetX = std::max(0.0f, (availSize.x - formWidth) * 0.5f);
            float offsetY = std::max(0.0f, (availSize.y - formHeight) * 0.5f);

            ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + offsetX, ImGui::GetCursorPosY() + offsetY));

            // Container Styling
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(50.0f, 50.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 7.0f);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, cardBg);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.15f, 0.15f, 0.16f, 1.0f));

            if(ImGui::BeginChild("ProjectSetupForm", ImVec2(formWidth, formHeight), true, ImGuiWindowFlags_NoScrollbar))
            {
                // Headers
                ImGui::PushFont(boldFont, 50.0f);
                const char* titleText = "Surge Engine";
                float titleWidth = ImGui::CalcTextSize(titleText).x;
                ImGui::SetCursorPosX((formWidth - titleWidth) * 0.5f);
                ImGui::TextColored(accentColor, titleText);
                ImGui::PopFont();

                ImGui::PushFont(regularFont, 19.0f);
                const char* subText = "Project Browser";
                float subWidth = ImGui::CalcTextSize(subText).x;
                ImGui::SetCursorPosX((formWidth - subWidth) * 0.5f);
                ImGui::TextColored(textMuted, subText);
                ImGui::PopFont();

                ImGui::Spacing();
                ImGui::Dummy(ImVec2(0.0f, 25.0f));

                float tabWidth = formWidth / 4.0f; // tabWidth is width of 1 tab
                float tabHeight = 40.0f;
                float totalTabsWidth = (tabWidth * 2.0f) + 8.0f; // 8px gap

                ImGui::SetCursorPosX((formWidth - totalTabsWidth) * 0.5f);

                // Dark background "Track" behind the tabs
                ImVec2 trackPos = ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddRectFilled(
                    ImVec2(trackPos.x - 4.0f, trackPos.y - 4.0f),
                    ImVec2(trackPos.x + totalTabsWidth + 4.0f, trackPos.y + tabHeight + 4.0f),
                    ImGui::ColorConvertFloat4ToU32(trackBg),
                    rounding
                );

                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 0.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, rounding); // Inner button rounding

                ImGui::PushFont(boldFont, 19.0f);
                // Tab 1: Create New
                ImGui::PushStyleColor(ImGuiCol_Button, sActiveTab == 0 ? accentColor : ImVec4(0.15f, 0.15f, 0.15f, 0));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, sActiveTab == 0 ? accentHover : inputBg);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, accentActive);
                ImGui::PushStyleColor(ImGuiCol_Text, sActiveTab == 0 ? ImVec4(0, 0, 0, 1) : textMuted);

                if(ImGui::Button("CREATE NEW", ImVec2(tabWidth, tabHeight)))
                    sActiveTab = 0;

                ImGui::PopStyleColor(4);
                ImGui::SameLine();

                // Tab 2: Open Existing
                ImGui::PushStyleColor(ImGuiCol_Button, sActiveTab == 1 ? accentColor : ImVec4(0.15f, 0.15f, 0.15f, 0));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, sActiveTab == 1 ? accentHover : inputBg);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, accentActive);
                ImGui::PushStyleColor(ImGuiCol_Text, sActiveTab == 1 ? ImVec4(0, 0, 0, 1) : textMuted);

                if(ImGui::Button("OPEN", ImVec2(tabWidth, tabHeight)))
                    sActiveTab = 1;

                ImGui::PopStyleColor(4);
                ImGui::PopStyleVar(2);
                ImGui::PopFont();

                ImGui::Dummy(ImVec2(0.0f, 25.0f));
                ImGui::Separator();
                ImGui::Dummy(ImVec2(0.0f, 15.0f));

                // TAB CONTENT
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16.0f, 12.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, rounding);
                ImGui::PushStyleColor(ImGuiCol_FrameBg, inputBg);
                ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, inputHovered);

                if(sActiveTab == 0)
                {
                    // Project Name
                    ImGui::PushFont(boldFont);
                    ImGui::TextColored(textActive, "PROJECT NAME");
                    ImGui::PopFont();
                    ImGui::Dummy(ImVec2(0.0f, 2.0f));

                    ImGui::SetNextItemWidth(-FLT_MIN);
                    char nameBuffer[256];
                    strncpy_s(nameBuffer, sTempProjectBuffer.Name.c_str(), sizeof(nameBuffer));
                    ImGui::PushFont(regularFont);
                    if(ImGui::InputText("##Name", nameBuffer, sizeof(nameBuffer)))
                        sTempProjectBuffer.Name = nameBuffer;
                    ImGui::PopFont();

                    ImGui::Dummy(ImVec2(0.0f, 15.0f));

                    // Directory
                    ImGui::PushFont(boldFont);
                    ImGui::TextColored(textActive, "DIRECTORY");
                    ImGui::PopFont();
                    ImGui::Dummy(ImVec2(0.0f, 2.0f));

                    ImGui::PushFont(boldFont);
                    float buttonWidth = ImGui::CalcTextSize(" BROWSE ").x + ImGui::GetStyle().FramePadding.x * 2.0f;
                    ImGui::PopFont();

                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - buttonWidth - ImGui::GetStyle().ItemSpacing.x);
                    char pathBuffer[512];
                    strncpy_s(pathBuffer, sCreateProjectPath.c_str(), sizeof(pathBuffer));

                    ImGui::PushFont(regularFont);
                    ImGui::InputText("##Filepath", pathBuffer, sizeof(pathBuffer), ImGuiInputTextFlags_ReadOnly);
                    ImGui::PopFont();

                    ImGui::SameLine();

                    ImGui::PushStyleColor(ImGuiCol_Button, inputBg);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, inputHovered);
                    ImGui::PushFont(boldFont);
                    if(ImGuiAux::Button(" BROWSE "))
                    {
                        String selectedPath = FileDialog::SaveFile("Surge Project (*.surgeproj)\0*.surgeproj\0All Files (*.*)\0*.*\0", nameBuffer);
                        if(!selectedPath.empty())
                            sCreateProjectPath = selectedPath;
                    }
                    ImGui::PopFont();
                    ImGui::PopStyleColor(2);

                    // Status Indicator
                    bool isProjectValid = sTempProjectBuffer.IsValid() && !sCreateProjectPath.empty();
                    ImGui::Dummy(ImVec2(0.0f, 15.0f));
                    ImGui::PushFont(regularFont);
                    if(isProjectValid)
                        ImGui::TextColored(accentColor, "Ready to CREATE!");
                    else
                        ImGui::TextDisabled("Required: Name & Directory");
                    ImGui::PopFont();

                    ImGui::SetCursorPosY(formHeight - 60.0f - ImGui::GetStyle().WindowPadding.y);

                    if(!isProjectValid)
                        ImGui::BeginDisabled();

                    ImGui::PushFont(boldFont, 23.0f);
                    ImGui::PushStyleColor(ImGuiCol_Button, accentColor);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, accentHover);
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, accentActive);
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));

                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, rounding);

                    if(ImGuiAux::Button("CREATE!", ImVec2(ImGui::GetContentRegionAvail().x, 50.0f)))
                        CreateProject();

                    ImGui::PopStyleVar();
                    ImGui::PopStyleColor(4);
                    ImGui::PopFont();

                    if(!isProjectValid)
                        ImGui::EndDisabled();
                }
                else if(sActiveTab == 1)
                {
                    // Manual Browse Section
                    ImGui::PushFont(boldFont);
                    ImGui::TextColored(textActive, "OPEN FROM DISK");
                    ImGui::PopFont();
                    ImGui::Dummy(ImVec2(0.0f, 2.0f));

                    ImGui::PushFont(boldFont);
                    float buttonWidth = ImGui::CalcTextSize(" BROWSE ").x + ImGui::GetStyle().FramePadding.x * 2.0f;
                    ImGui::PopFont();

                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - buttonWidth - ImGui::GetStyle().ItemSpacing.x);

                    char openPathBuffer[512];
                    strncpy_s(openPathBuffer, sOpenProjectPath.c_str(), sizeof(openPathBuffer));

                    ImGui::PushFont(regularFont);
                    ImGui::InputText("##OpenPath", openPathBuffer, sizeof(openPathBuffer), ImGuiInputTextFlags_ReadOnly);
                    ImGui::PopFont();

                    ImGui::SameLine();

                    ImGui::PushStyleColor(ImGuiCol_Button, inputBg);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, inputHovered);
                    ImGui::PushFont(boldFont);
                    if(ImGuiAux::Button(" BROWSE "))
                    {
                        String selectedPath = FileDialog::OpenFile("Surge Project (*.surgeproj)\0*.surgeproj\0All Files (*.*)\0*.*\0");
                        if(!selectedPath.empty())
                            sOpenProjectPath = selectedPath;
                    }
                    ImGui::PopFont();
                    ImGui::PopStyleColor(2);

                    ImGui::Dummy(ImVec2(0.0f, 20.0f));

                    // Recent Projects Section
                    ImGui::PushFont(boldFont);
                    ImGui::TextColored(textActive, "RECENT PROJECTS");
                    ImGui::PopFont();
                    ImGui::Dummy(ImVec2(0.0f, 5.0f));

                    // Calculate available height, leaving room for the bottom "Open Project" button
                    float listHeight = ImGui::GetContentRegionAvail().y - 100.0f;

                    ImGui::PushStyleColor(ImGuiCol_ChildBg, listBg);
                    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);

                    if(ImGui::BeginChild("RecentsList", ImVec2(0, listHeight), true))
                    {
                        if (sRecentProjects.empty())
                        {
                            ImGui::PushFont(boldFont, 20.0f);
                            ImGui::PushStyleColor(ImGuiCol_Text, textMuted);
                            ImGuiAux::TextCentered("No recent projects");
                            ImGui::PopStyleColor();
                            ImGui::PopFont();
                        }
                        else
                        {
                            for(size_t i = 0; i < sRecentProjects.size(); ++i)
                            {
                                const auto& proj = sRecentProjects[i];
                                bool isSelected = (sOpenProjectPath == proj.Filepath);

                                ImGui::PushID(static_cast<int>(i));

                                // Override Selectable colors (Header, HeaderHovered, HeaderActive)
                                ImGui::PushStyleColor(ImGuiCol_Header, selectedBlack);
                                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, hoverGray);
                                ImGui::PushStyleColor(ImGuiCol_HeaderActive, activeGray);

                                ImVec2 cursorPos = ImGui::GetCursorPos();

                                // Render an invisible selectable that covers the whole row
                                if(ImGui::Selectable("##recent", isSelected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0, 55.0f)))
                                {
                                    sOpenProjectPath = proj.Filepath;
                                    if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                                        OpenProject();
                                }

                                ImVec2 finalCursorPos = ImGui::GetCursorPos();

                                // Overlay the text elements manually on top of the selectable
                                ImGui::SetCursorPos(ImVec2(cursorPos.x + 15.0f, cursorPos.y + 8.0f));
                                ImGui::PushFont(boldFont);
                                ImGui::TextColored(textActive, "%s", proj.Name.c_str());
                                ImGui::PopFont();

                                ImGui::SetCursorPos(ImVec2(cursorPos.x + 15.0f, cursorPos.y + 32.0f));
                                ImGui::PushFont(regularFont);
                                ImGui::TextColored(isSelected ? ImVec4(0.9f, 0.9f, 0.9f, 1.0f) : textMuted, "%s", proj.Filepath.c_str());
                                ImGui::PopFont();

                                // Right-aligned "Last Opened" timestamp
                                ImGui::PushFont(regularFont);
                                float timeWidth = ImGui::CalcTextSize(proj.LastOpened.c_str()).x;
                                ImGui::SetCursorPos(ImVec2(ImGui::GetContentRegionMax().x - timeWidth - 15.0f, cursorPos.y + 8.0f));
                                ImGui::TextColored(isSelected ? ImVec4(0.9f, 0.9f, 0.9f, 1.0f) : textMuted, "%s", proj.LastOpened.c_str());
                                ImGui::PopFont();

                                // Restore cursor for the next item
                                ImGui::SetCursorPos(finalCursorPos);
                                ImGui::Dummy(ImVec2(0.0f, 0.0f));

                                ImGui::PopStyleColor(3);
                                ImGui::PopID();
                            }
                        }
                    }
                    ImGui::EndChild();
                    ImGui::PopStyleVar();
                    ImGui::PopStyleColor();

                    // Action Button
                    ImGui::SetCursorPosY(formHeight - 60.0f - ImGui::GetStyle().WindowPadding.y);

                    bool isOpenValid = !sOpenProjectPath.empty();

                    // Small status text right above the button
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 25.0f);
                    ImGui::PushFont(regularFont);
                    if(!sOpenProjectPath.empty())
                        ImGui::TextColored(accentColor, "Ready to Open!");
                    else
                        ImGui::TextDisabled("Please select a valid project file");
                    ImGui::PopFont();

                    ImGui::SetCursorPosY(formHeight - 60.0f - ImGui::GetStyle().WindowPadding.y);

                    if(!isOpenValid)
                        ImGui::BeginDisabled();

                    ImGui::PushFont(boldFont, 23.0f);
                    ImGui::PushStyleColor(ImGuiCol_Button, accentColor);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, accentHover);
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, accentActive);
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, rounding);

                    if(ImGuiAux::Button("OPEN PROJECT", ImVec2(ImGui::GetContentRegionAvail().x, 50.0f)))
                        OpenProject();

                    ImGui::PopStyleVar();
                    ImGui::PopStyleColor(4);
                    ImGui::PopFont();

                    if(!isOpenValid)
                        ImGui::EndDisabled();
                }
                ImGui::PopStyleColor(2);
                ImGui::PopStyleVar(2);
            }
            ImGui::EndChild();

            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(2);
        }
        ImGui::End();

        ImGui::PopStyleColor(); // WindowBg
        ImGui::PopStyleVar(2); // BorderSize, Rounding
    }

    void ProjectBrowser::CreateProject()
    {
        auto* editor = static_cast<Editor*>(Core::GetClient());

        // Create Assets directory and initialize AssetManager with it
        const Path assetManagerPath = Filesystem::GetParentPath(sCreateProjectPath) / "Assets";
        Filesystem::CreateOrEnsureDirectory(assetManagerPath);
        AssetManager::Shutdown();
        AssetManager::Initialize(assetManagerPath);
        editor->GetPanelManager().GetPanel<ContentBrowserPanel>()->OnAssetManagerInit();

        // Create Start Scene and serialize it to the new project's Assets folder, then import it to get an AssetID reference
        Ref<Scene> newScene = Ref<Scene>::Create();

        const String defaultScenePath = "Scenes/Main.srg";
        Filesystem::CreateOrEnsureDirectory(assetManagerPath / "Scenes");
        Serializer::SerializeScene(AssetManager::GetAbsolutePath(defaultScenePath), newScene.Raw());
        sTempProjectBuffer.StartScene = AssetManager::ImportLive(defaultScenePath, AssetType::SCENE, newScene);

        // Serialize the new project file
        Serializer::SerializeProject(sCreateProjectPath, &sTempProjectBuffer);

        // Setup editor context with the new project and scene
        editor->LoadScene(std::move(newScene));
        editor->SetCurrentProject(sTempProjectBuffer);

        sTempProjectBuffer.Clear();
        sCreateProjectPath.clear();
        Core::GetWindow()->Maximize();
    }

    void ProjectBrowser::OpenProject()
    {
        auto* editor = static_cast<Editor*>(Core::GetClient());

        // Get the opened project data
        Project openedProject;
        Serializer::DeserializeProject(sOpenProjectPath, &openedProject);

        // Initialize AssetManager with the opened project's Assets directory
        const Path assetManagerPath = Filesystem::GetParentPath(sOpenProjectPath) / "Assets";
        AssetManager::Shutdown();
        AssetManager::Initialize(assetManagerPath);
        editor->GetPanelManager().GetPanel<ContentBrowserPanel>()->OnAssetManagerInit();

        // Load the start scene of the opened project
        Ref<Scene> loadedScene = AssetManager::Load<Scene>(openedProject.StartScene);
        SG_ASSERT(loadedScene, "Failed to load start scene!");

        // Setup editor context with the new project and scene
        editor->LoadScene(std::move(loadedScene));
        editor->SetCurrentProject(openedProject);

        sOpenProjectPath.clear();
        Core::GetWindow()->Maximize();
    }

} // namespace Surge
