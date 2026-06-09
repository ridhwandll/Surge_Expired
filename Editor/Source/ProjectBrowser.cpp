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
#include "Editor.hpp"

#include <imgui.h>
#include <Panels/ContentBrowserPanel.hpp>
#include "Surge/Utility/Platform.hpp"
#include <json/json.hpp>
#include <fstream>

#define RECENT_PROJECTS_FILENAME "RecentProjects.json"
#define MAX_RECENT_PROJECTS 10

namespace Surge
{
    // TODO: Populate this list from a config file that stores recent projects
    static Vector<RecentProject> sRecentProjects = {};

    static Project sTempProjectBuffer;
    static String sOpenProjectPath;
    static String sCreateProjectPath;
    static int sActiveTab = 1; // 0 = CREATE PROJECT, 1 = OPEN PROJECT

    void ProjectBrowser::Init()
    {
        mAssetManager = Core::GetAssetManager();
        DeserializeRecentProjects();
    }

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
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
                    if(ImGui::BeginChild("RecentsList", ImVec2(0, listHeight), true))
                    {
                        if(sRecentProjects.empty())
                        {
                            constexpr float emptyFontSize = 30.0f;
                            ImGui::Dummy(ImVec2(0.0f, listHeight * 0.5f - emptyFontSize));
                            ImGui::PushFont(boldFont, emptyFontSize);
                            ImGui::PushStyleColor(ImGuiCol_Text, textMuted);
                            ImGuiAux::TextCentered("No PROJECTS to show");
                            ImGui::PopStyleColor();
                            ImGui::PopFont();
                        }
                        else
                        {
                            int projectToDelete = -1;

                            for(size_t i = 0; i < sRecentProjects.size(); ++i)
                            {
                                const auto& proj = sRecentProjects[i];
                                bool isSelected = (sOpenProjectPath == proj.Filepath);

                                ImGui::PushID(static_cast<int>(i));

                                ImGui::PushStyleColor(ImGuiCol_Header, selectedBlack);
                                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, hoverGray);
                                ImGui::PushStyleColor(ImGuiCol_HeaderActive, activeGray);

                                constexpr float kRowH = 67.0f;
                                constexpr float kLeftPad = 18.0f;
                                constexpr float kRightPad = 14.0f;

                                const ImVec2 screenPos = ImGui::GetCursorScreenPos();
                                const ImVec2 cursorPos = ImGui::GetCursorPos();
                                const float availW = ImGui::GetContentRegionAvail().x;
                                const float rightEdge = cursorPos.x + availW - kRightPad;

                                if(ImGui::Selectable("##Proj", isSelected, ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_AllowOverlap, ImVec2(0, kRowH)))
                                {
                                    sOpenProjectPath = proj.Filepath;
                                    if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                                        OpenProject();
                                }
                                const ImVec2 postCursor = ImGui::GetCursorPos();

                                if(isSelected)
                                {
                                    ImGui::GetWindowDrawList()->AddRectFilled(
                                        ImVec2(screenPos.x, screenPos.y + 6.0f),
                                        ImVec2(screenPos.x + 8.0f, screenPos.y + kRowH - 6.0f),
                                        ImGui::ColorConvertFloat4ToU32(ImGuiAux::Colors::ThemeColor2), 2.0f);
                                }

                                const float textX = cursorPos.x + kLeftPad;

                                // Left column
                                // Project name
                                ImGui::SetCursorPos(ImVec2(textX, cursorPos.y + 12.0f));
                                ImGui::PushFont(boldFont, 23.0f);
                                ImGui::TextColored(isSelected ? ImVec4(ImGuiAux::Colors::ThemeColor2) : textActive, "%s", proj.Name.c_str());
                                ImGui::PopFont();

                                // Filepath
                                ImGui::SetCursorPos(ImVec2(textX, cursorPos.y + 38.0f));
                                ImGui::PushFont(regularFont);
                                ImGui::TextColored(textMuted, "%s", proj.Filepath.c_str());
                                ImGui::PopFont();

                                // Right column
                                // Last Opened
                                const float timeW = ImGui::CalcTextSize(proj.LastOpened.c_str()).x;
                                ImGui::SetCursorPos(ImVec2(rightEdge - timeW, cursorPos.y + 13.0f));
                                ImGui::PushFont(regularFont);
                                ImGui::TextColored(textMuted, "%s", proj.LastOpened.c_str());
                                ImGui::PopFont();

                                // DELETE
                                ImGui::PushFont(boldFont);
                                const float deleteW = ImGui::CalcTextSize("DELETE").x + ImGui::GetStyle().FramePadding.x;
                                const float deleteH = 22.0f;
                                ImGui::SetCursorPos(ImVec2(rightEdge - deleteW, cursorPos.y + 36.0f));
                                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.16f, 0.16f, 0.80f));
                                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.90f, 0.10f, 0.10f, 1.00f));
                                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
                                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);

                                if(ImGui::Button("DELETE", ImVec2(deleteW, deleteH)))
                                    projectToDelete = static_cast<int>(i);

                                ImGui::PopStyleVar(2);
                                ImGui::PopStyleColor(3);
                                ImGui::PopFont();

                                // Separator skip on last item
                                if(i + 1 < sRecentProjects.size())
                                {
                                    ImGui::GetWindowDrawList()->AddLine(
                                        ImVec2(screenPos.x + 10.0f, screenPos.y + kRowH),
                                        ImVec2(screenPos.x + availW - 10.0f, screenPos.y + kRowH),
                                        ImGui::ColorConvertFloat4ToU32(ImGuiAux::Colors::ThemeColor2), 1.0f);
                                }

                                ImGui::SetCursorPos(postCursor);
                                ImGui::Dummy(ImVec2(0, 1.0f));

                                ImGui::PopStyleColor(3);
                                ImGui::PopID();
                            }
                            if(projectToDelete != -1)
                            {
                                if(sOpenProjectPath == sRecentProjects[projectToDelete].Filepath)
                                    sOpenProjectPath.clear();

                                sRecentProjects.erase(sRecentProjects.begin() + projectToDelete);
                                SerializeRecentProjects();
                            }
                        }
                    }
                    ImGui::EndChild();
                    ImGui::PopStyleVar(2);
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
        Filesystem::CreateOrEnsureDirectories(assetManagerPath);
        mAssetManager->Shutdown();
        mAssetManager->Initialize(assetManagerPath);
        editor->GetPanelManager().GetPanel<ContentBrowserPanel>()->OnAssetManagerInit();

        // Create Start Scene and serialize it to the new project's Assets folder, then import it to get an AssetID reference
        const String defaultScenePath = "Scenes/Main.srg";
        Filesystem::CreateOrEnsureDirectories(assetManagerPath / "Scenes");
        Ref<Scene> newScene = mAssetManager->Create<Scene>(defaultScenePath);
        sTempProjectBuffer.StartScene = newScene->GetID();

        // Setup editor context with the new project and scene
        editor->LoadScene(std::move(newScene));
        editor->SetCurrentProject(sTempProjectBuffer);

        Serializer::SerializeProject(sCreateProjectPath, &sTempProjectBuffer);
        SerializeRecentProject(sTempProjectBuffer.Name, sCreateProjectPath);

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
        mAssetManager->Shutdown();
        mAssetManager->Initialize(assetManagerPath);
        editor->GetPanelManager().GetPanel<ContentBrowserPanel>()->OnAssetManagerInit();

        // Load the start scene of the opened project
        Ref<Scene> loadedScene = mAssetManager->Load<Scene>(openedProject.StartScene);
        SG_ASSERT(loadedScene, "Failed to load start scene!");

        // Setup editor context with the new project and scene
        editor->LoadScene(std::move(loadedScene));
        editor->SetCurrentProject(openedProject);

        SerializeRecentProject(openedProject.Name, sOpenProjectPath);

        sOpenProjectPath.clear();
        Core::GetWindow()->Maximize();
    }

    inline void to_json(nlohmann::json& j, const RecentProject& p)
    {
        j = nlohmann::json {
            {"Name", p.Name},
            {"Filepath", p.Filepath},
            {"LastOpened", p.LastOpened}
        };
    }

    inline void from_json(const nlohmann::json& j, RecentProject& p)
    {
        j.at("Name").get_to(p.Name);
        j.at("Filepath").get_to(p.Filepath);
        j.at("LastOpened").get_to(p.LastOpened);
    }

    void ProjectBrowser::SerializeRecentProject(const String& name, const String& filepath)
    {
        RecentProject proj;
        proj.Name = name;
        proj.Filepath = Path(filepath).generic_string();
        proj.LastOpened = GetTimeString();

        sRecentProjects.erase(std::remove_if(sRecentProjects.begin(), sRecentProjects.end(), [&](const RecentProject& p) { return p.Filepath == proj.Filepath; }), sRecentProjects.end());
        sRecentProjects.insert(sRecentProjects.begin(), proj);
        SerializeRecentProjects();
    }

    void ProjectBrowser::SerializeRecentProjects()
    {
        if(sRecentProjects.size() > MAX_RECENT_PROJECTS)
            sRecentProjects.pop_back();

        String storePath = Platform::GetPersistantStoragePath() + "/" + RECENT_PROJECTS_FILENAME;
        nlohmann::json j;

        j["INFO"] = "Generated by SurgeEngine Editor on " + GetTimeString();
        j["RecentProjects"] = sRecentProjects;

        std::ofstream file(storePath);
        if(file.is_open())
        {
            file << j.dump(4);
            file.close();
        }
    }

    void ProjectBrowser::DeserializeRecentProjects()
    {
        String storePath = Platform::GetPersistantStoragePath() + "/" + RECENT_PROJECTS_FILENAME;
        if(!Filesystem::Exists(storePath))
            return;

        std::ifstream file(storePath);
        if(file.is_open())
        {
            try
            {
                nlohmann::json j;
                file >> j;

                if(j.contains("RecentProjects") && j["RecentProjects"].is_array())
                {
                    sRecentProjects.clear();

                    for(const auto& item : j["RecentProjects"])
                    {
                        auto proj = item.get<RecentProject>();

                        if(Filesystem::Exists(proj.Filepath.c_str()))
                            sRecentProjects.push_back(proj);
                    }

                    // If we found and skipped dead projects, update the JSON file automatically
                    if(sRecentProjects.size() < j["RecentProjects"].size())
                        SerializeRecentProjects();
                }
            }
            catch(const nlohmann::json::exception& e)
            {
                sRecentProjects.clear();
            }
            file.close();
        }
    }

    String ProjectBrowser::GetTimeString()
    {
        auto now = std::chrono::system_clock::now();
        std::time_t nowTime = std::chrono::system_clock::to_time_t(now);

        std::tm local_tm;
        localtime_s(&local_tm, &nowTime);

        std::stringstream ss;
        ss << std::put_time(&local_tm, "%b %d, %Y // %I:%M %p");

        return ss.str();
    }

} // namespace Surge
