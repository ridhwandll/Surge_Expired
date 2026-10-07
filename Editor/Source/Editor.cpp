// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Editor.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Core/Time/Clock.hpp"
#include "Surge/Core/Input/Input.hpp"

#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Physics/Physics.hpp"

#include "Surge/Utility/Platform.hpp"
#include "Surge/Utility/Filesystem.hpp"

#include "Utility/ImGuiAux.hpp"
#include "Panels/ViewportPanel.hpp"
#include "Panels/SceneHierarchyPanel.hpp"
#include "Panels/InspectorPanel.hpp"
#include "Panels/ContentBrowserPanel.hpp"
#include "Panels/MaterialEditorPanel.hpp"
#include "Panels/ExportPanel.hpp"
#include "Panels/UIEditorPanel.hpp"
#include "Panels/ConsolePanel.hpp"
#include "Panels/HistoryPanel.hpp"
#include "Console/EditorConsole.hpp"
#include "Surge/ECS/Components/Components.hpp"

#include "Asset/Cookers/Texture2DCooker.hpp"
#include "Asset/Cookers/MeshCooker.hpp"
#include "Asset/Cookers/MaterialCooker.hpp"
#include "Asset/Cookers/ScriptCooker.hpp"
#include "Asset/Cookers/FontCooker.hpp"
#include "Asset/Cookers/AudioCooker.hpp"

#include <algorithm>
#include <cstdio>
#include <mutex>


namespace Surge
{
    static ImageHandle GenerateImage(const String& path)
    {
        Renderer* renderer = Core::GetRenderer();
        Scope<GraphicsRHI>& rhi = renderer->GetRHI();

        TextureSpecification spec = Texture2DCooker::LoadFromSource(path);
        ImageDesc desc = {};
        desc.Format = ImageFormat::RGBA8_UNORM;
        desc.Usage = ImageUsage::SAMPLED | ImageUsage::TRANSFER_DST;
        desc.GenerateImGuiID = true;
        desc.Sampler = renderer->GetDefaultSampler();

        desc.Width = spec.Width;
        desc.Height = spec.Height;
        desc.DebugName = Filesystem::GetFilenameWithExt(path);
        desc.InitialData = spec.Content;
        desc.DataSize = spec.Width * spec.Height * 4;
        ImageHandle imageHandle = rhi->CreateImage(desc);
        Texture2DCooker::FreeLoadedSource(spec);

        return imageHandle;
    }

    class EditorAssetLoadCallback : public AssetLoadCallback
    {
    protected:
        virtual bool OnAssetLoad(AssetID id, AssetMetadata& meta) override
        {
            Editor* editor = static_cast<Editor*>(Core::GetClient());
            if(editor->GetAssetImporter().NeedsCook(id, meta.Type))
            {
                Log<Severity::Trace>("[EditorAssetLoadCallback] Cooked file outdated/missing on Load, cooking now: {}", meta.RelativePath);
                editor->GetAssetImporter().RecookAsset(id);
                return true; // Reload the asset from disk after cooking
            }
            return false;
        }
    };

    void Editor::OnInitialize()
    {
        // Dummy Scene to render the project browser ImGui. We can probably find a better way to do this later, but for now it works and doesn't cause any issues
        mActiveScene = Ref<Scene>::Create();

        mAssetManager = Core::GetAssetManager();
        mRenderer = Core::GetRenderer();
        mRenderer->SetOutlineThickness(1);

        mCamera = EditorCamera(45.0f, 1.778f, 0.1f, 1000.0f);
        mCamera.SetActive(true);

        mAssetImporter.Initialize(mAssetManager);
        mAssetImporter.RegisterCooker(CreateScope<Texture2DCooker>());
        mAssetImporter.RegisterCooker(CreateScope<MaterialCooker>());
        mAssetImporter.RegisterCooker(CreateScope<MeshCooker>());
        mAssetImporter.RegisterCooker(CreateScope<ScriptCooker>());
        mAssetImporter.RegisterCooker(CreateScope<FontCooker>());
        mAssetImporter.RegisterCooker(CreateScope<AudioCooker>());

        mAssetManager->AddAssetLoadCallback(CreateScope<EditorAssetLoadCallback>());

        // Configure panels
        SceneHierarchyPanel* sceneHierarchy = mPanelManager.PushPanel<SceneHierarchyPanel>();
        mPanelManager.PushPanel<InspectorPanel>()->SetHierarchy(sceneHierarchy);
        mViewportPanel = mPanelManager.PushPanel<ViewportPanel>(&mCamera);
        mPanelManager.PushPanel<ContentBrowserPanel>();
        mPanelManager.PushPanel<MaterialEditorPanel>();
        mPanelManager.PushPanel<ExportPanel>();
        mPanelManager.PushPanel<UIEditorPanel>();
        mPanelManager.PushPanel<ConsolePanel>();
        mPanelManager.PushPanel<HistoryPanel>();

        mHistory.SetScene(mActiveScene.Raw());

        mProjectBrowser.Init();

        mRenderer->AddImGuiRenderCallback([this]() { OnImGuiRender(); });

        mEngineLogo = GenerateImage("Editor/Assets/Textures/EngineLogo.png");
        mMinimize = GenerateImage("Editor/Assets/Textures/Minimize.png");
        mMaximize = GenerateImage("Editor/Assets/Textures/Maximize.png");
        mClose = GenerateImage("Editor/Assets/Textures/Close.png");
    }

    void Editor::OnUpdate()
    {
        if(mCurrentProject.IsValid())
        {
            // Axes
            constexpr float axesLength = 10000.0f;
            if (mShowAxes)
            {
                mRenderer->SubmitLine({ -axesLength, 0.0f, 0.0f }, { axesLength, 0.0f, 0.0f }, { 1.0f, 0.3f, 0.3f, 1.0f }); // X
                mRenderer->SubmitLine({ 0.0f, -axesLength, 0.0f }, { 0.0f, axesLength, 0.0f }, { 0.3f, 0.8f, 0.3f, 1.0f }); // Y
                mRenderer->SubmitLine({ 0.0f, 0.0f, -axesLength }, { 0.0f, 0.0f, axesLength }, { 0.3f, 0.3f, 1.0f, 1.0f }); // Z
            }

            CheckResize();
            if(mRuntimeScene && mRuntimeScene->GetMainCameraEntity().Data1)
                mRuntimeScene->Update();
            else
            {
                mCamera.OnUpdate();
                mActiveScene->Update(mCamera);
            }
        }
        else
        {
            // (Rid) We have to do this to render the ImGUI! Is this a design flaw? Maybe. But it works for now and we can refactor later if needed
            mActiveScene->Update(mCamera);
        }
    }

    void Editor::OnImGuiRender()
    {
        constexpr float titleBarHeight = 65.0f;

        if(mCurrentProject.IsValid())
        {
            mRenderer->ShowInternalImGui(true);
            HandleShortcuts();

            const char* unsavedMarker = mHistory.IsDirty() ? " *" : "";
#ifdef SURGE_DEBUG
            DrawCustomTitlebar(("[DEBUG] Surge Editor //" + mCurrentProject.Name + unsavedMarker).c_str(), titleBarHeight);
#elif defined(SURGE_RELEASE)
            DrawCustomTitlebar(("Surge Editor //" + mCurrentProject.Name + unsavedMarker).c_str(), titleBarHeight);
#endif
            ImGuiAux::DockSpace(titleBarHeight);
            mPanelManager.RenderPanels();
            RenderEditorSettings();
            RenderShortcutsWindow();

            // Every panel had its chance to modify the selected entity, an interaction that just ended becomes an undo step
            if(!IsPlaying())
                mHistory.TrackSelection(GetSelectedEntity(), ImGui::IsAnyItemActive() || ImGui::IsMouseDown(ImGuiMouseButton_Left));

            ImGui::Begin("Physics Stats");

            Surge::Physics* physics = Core::GetPhysics();
            if(physics)
            {
                int total = 0;
                int active = 0;
                physics->GetDebugStats(active, total);

                ImGui::Text("Total Bodies: %d", total);
                ImGui::Text("Active Bodies: %d", active);
                ImGui::Text("Sleeping Bodies: %d", total - active);
            }

            ImGui::End();
        }
        else
        {
            constexpr float projectWindowTitlebarHeight = 40.0f;
            mRenderer->ShowInternalImGui(false);
            DrawCustomTitlebar("Project Browser", projectWindowTitlebarHeight, false);
            mProjectBrowser.Render(projectWindowTitlebarHeight);
        }
        ImGuiAux::RenderConfirmationBox();
    }

    void Editor::DrawCustomTitlebar(const char* title, float titleBarHeight, bool showMenuItems)
    {
        if (!mShowTitlebar)
            return;

        ImGuiViewport* viewport = ImGui::GetMainViewport();

        ImVec4 bgColor = ImGuiAux::Colors::ExtraDark;
        ImVec4 textColor = ImGuiAux::Colors::White;
        if(IsPlaying())
        {
            float time = static_cast<float>(ImGui::GetTime());
            float pulse = (glm::sin(time * 2.0f) + 1.0f) * 0.5f;

            glm::vec4 bgBright = ImGuiAux::Colors::ThemeColor2;
            glm::vec4 bgDark = ImGuiAux::Colors::ExtraDark;

            glm::vec4 txtBright = ImGuiAux::Colors::White;
            glm::vec4 txtDark = bgDark;

            bgColor = glm::mix(bgDark, bgBright, pulse);
            textColor = glm::mix(txtBright, txtDark, pulse);
        }

        ImGui::PushStyleColor(ImGuiCol_WindowBg, bgColor);
        ImGui::PushStyleColor(ImGuiCol_Text, textColor);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, titleBarHeight));

        constexpr ImGuiWindowFlags titlebarFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollWithMouse;

        if(ImGui::Begin("##SurgeTitleBar", nullptr, titlebarFlags))
        {
            ImFont* boldFont = ImGui::GetIO().Fonts->Fonts[1];
            float availableWidth = ImGui::GetContentRegionAvail().x;
            constexpr float topPadding = 10.0f;
            float currentX = 10.0f; // Left padding

            // ENGINE ICON
            float logoWidthHeight = showMenuItems ? 50.0f : 28.0f;
            float logoYPos = showMenuItems ? topPadding : ((titleBarHeight - logoWidthHeight) * 0.5f);

            ImGui::SetCursorPos(ImVec2(currentX, logoYPos));
            ImTextureID engineLogoTextureID = mRenderer->GetRHI()->GetImGuiImage(mEngineLogo);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0)); // Transparent button
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
            if(ImGui::ImageButton("EngineIcon", engineLogoTextureID, ImVec2(logoWidthHeight, logoWidthHeight)))
                ConfirmDiscardChanges("Leave the project", [this]() { mCurrentProject = {}; }); // Reset project to go back to the Project Browser
            ImGui::PopStyleColor(2);
            currentX += logoWidthHeight + 5.0f;

            // EDITOR MENUS & PLAYBACK
            if(showMenuItems)
            {
                // MENUS
                ImGui::SetCursorPos(ImVec2(currentX, topPadding));
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0)); // Transparent button
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.1f));
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 6.0f));

                if(ImGui::Button("EDIT"))
                    ImGui::OpenPopup("EditMenu");
                ImGui::SameLine();
                if(ImGui::Button("VIEW"))
                    ImGui::OpenPopup("WindowMenu");
                ImGui::SameLine();
                if(ImGui::Button("HELP"))
                    mShowShortcutsWindow = true;

                ImGui::PopStyleVar();
                ImGui::PopStyleColor(2);

                currentX = ImGui::GetCursorPosX();

                // Menu Popups
                ImGuiAux::StyledPopupVars::Push();
                ImGui::SetCursorPosY(titleBarHeight);
                if(ImGui::BeginPopup("WindowMenu", ImGuiPopupFlags_None))
                {
                    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]); // Bold font
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "EDITOR PANELS");
                    ImGui::PopFont();

                    ImGuiAux::StyledSeparator();

                    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.18f, 0.18f, 0.18f, 1.0f));
                    for(auto& [panelCode, panel] : mPanelManager.GetAllPanels())
                        ImGui::MenuItem(PanelCodeToString(panelCode), nullptr, &panel.Show);

                    ImGuiAux::StyledSeparator();

                    if(ImGui::MenuItem("Reset Default Layout"))
                    {
                        for(auto& [panelCode, panel] : mPanelManager.GetAllPanels())
                            panel.Show = true;
                        // TODO: ImGui::LoadIniSettingsFromDisk("imgui_default.ini");
                    }
                    ImGui::PopStyleColor();
                    ImGui::EndPopup();
                }
                DrawEditMenu();
                ImGuiAux::StyledPopupVars::Pop();

                // PLAYBACK CONTROLS (BOTTOM ROW)
                float buttonWidth = 40.0f; // Width to fit "STOP" and "PLAY"
                float playControlsX = (availableWidth - buttonWidth) * 0.5f;

                ImGui::PushFont(boldFont);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0)); // Transparent button
                if(playControlsX > currentX + 50.0f)
                {
                    ImGui::SetCursorPos(ImVec2(playControlsX, 36.0f));

                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 2.0f));

                    bool isPlaying = IsPlaying();
                    if(isPlaying)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImGuiAux::Colors::Red);
                        if(ImGui::Button("STOP", ImVec2(buttonWidth, 0)))
                            OnRuntimeEnd();
                        ImGui::PopStyleColor();
                    }
                    else
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImGuiAux::Colors::ThemeColor1);
                        if(ImGui::Button("PLAY", ImVec2(buttonWidth, 0)))
                            OnRuntimeStart();
                        ImGui::PopStyleColor();
                    }

                    ImGui::PopStyleVar(2);
                }
                ImGui::PopStyleColor();
                ImGui::PopFont();
            }

            // TITLE
            constexpr float titleFontSize = 19.0f;
            ImGui::PushFont(boldFont, titleFontSize);
            float textWidth = ImGui::CalcTextSize(title).x;
            float centerPosX = (availableWidth - textWidth) * 0.5f;
            float titleYPos = showMenuItems ? topPadding : ((titleBarHeight - titleFontSize) * 0.5f);

            // Prevent overlap with menus/logo if the window gets squished
            if(centerPosX > currentX + 20.0f)
            {
                ImGui::SetCursorPos(ImVec2(centerPosX, titleYPos));
                ImGui::Text("%s", title);
            }
            ImGui::PopFont();

            // WINDOW CONTROLS
            constexpr float controlsWidth = 120.0f;
            constexpr float rightPadding = 10.0f;

            // Button height is 24px + 6px + 6px = 36px. Center it perfectly when in 40px mode.
            float windowTopPadding = showMenuItems ? topPadding : ((titleBarHeight - 36.0f) * 0.5f);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0)); // Transparent buttons
            ImGui::SetCursorPos(ImVec2(availableWidth - controlsWidth - rightPadding, windowTopPadding));

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
            if(ImGui::ImageButton("##Minimize", mRenderer->GetRHI()->GetImGuiImage(mMinimize), ImVec2(24.0f, 24.0f)))
                Core::GetWindow()->Minimize();

            ImGui::SameLine();

            if(ImGui::ImageButton("##Maximize", mRenderer->GetRHI()->GetImGuiImage(mMaximize), ImVec2(24.0f, 24.0f)))
            {
                Window* window = Core::GetWindow();
                if(window->IsWindowMaximized())
                    window->RestoreFromMaximize();
                else
                    window->Maximize();
            }
            ImGui::PopStyleColor();

            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
            if(ImGui::ImageButton("##Close", mRenderer->GetRHI()->GetImGuiImage(mClose), ImVec2(24.0f, 24.0f)))
                ConfirmDiscardChanges("Exit", []() { Platform::RequestExit(); });

            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);

            // FPS
            if(showMenuItems)
            {
                static float sUpdateTimer = 0.0f;
                static String sFpsText = "0.00 ms | 0 FPS";

                sUpdateTimer += ImGui::GetIO().DeltaTime;
                if(sUpdateTimer >= 0.25f)
                {
                    const float frameTimeMs = Core::GetClock().GetMilliseconds();
                    const float fps = ImGui::GetIO().Framerate;
                    sFpsText = std::format("{:.2f} ms | {:.0f} FPS", frameTimeMs, fps);
                    sUpdateTimer = 0.0f;
                }

                const float fpsWidth = ImGui::CalcTextSize(sFpsText.c_str()).x;

                ImGui::SetCursorPos(ImVec2(availableWidth - fpsWidth - rightPadding, 48.0f));
                ImGui::Text("%s", sFpsText.c_str());

                DrawConsoleCounters(availableWidth - fpsWidth - rightPadding - 16.0f, 48.0f);
            }
        }
        ImGui::End();

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
    }

    void Editor::RenderEditorSettings()
    {
        ImGui::Begin("Editor Settings");
        ImGui::Checkbox("Show Axes", &mShowAxes);

        static float mFrameCap = 144.0f;
        if(ImGui::DragFloat("Cap FPS", &mFrameCap, 20.0f, 0.0f, 1000.0f))
            Core::SetFrameCap(mFrameCap);

        if (ImGui::Button("Toggle Debug Render"))
        {
            if(mRuntimeScene)
                mRuntimeScene->SetRenderDebug(!mRuntimeScene->IsRenderDebug());
            else
                mActiveScene->SetRenderDebug(!mActiveScene->IsRenderDebug());
        }
        ImGui::End();
    }

    void Editor::OnEvent(Event& e)
    {
        if (mViewportPanel->IsViewportHovered())
            mCamera.OnEvent(e);

        mPanelManager.OnEvent(e);

        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<KeyPressedEvent>([&](KeyPressedEvent& keyEvent)
                                             {
                                                 if(keyEvent.GetKeyCode() == Key::F5)
                                                 {
                                                     if(IsPlaying())
                                                         OnRuntimeEnd();
                                                     else
                                                         OnRuntimeStart();
                                                 }
                                             });
    }

    void Editor::OnRuntimeStart()
    {
        mHistory.FlushTracked(); // Commit pending edits
        mHistory.ResetTracking();
        if(mPanelManager.GetPanel<ConsolePanel>()->IsClearOnPlay())
            EditorConsole::Get().Clear();

        mRuntimeScene = Ref<Scene>::Create();
        mPanelManager.GetPanel<SceneHierarchyPanel>()->SetSceneContext(mRuntimeScene.Raw());
        mPanelManager.GetPanel<ViewportPanel>()->OnSceneContextChanged();

        mActiveScene->CopyTo(mRuntimeScene.Raw());

        mRuntimeScene->OnRuntimeStart();
        glm::vec2 viewportSize = mPanelManager.GetPanel<ViewportPanel>()->GetViewportSize();
        mRuntimeScene->OnResize(viewportSize.x, viewportSize.y);

        mShowAxes = false;
    }

    void Editor::OnRuntimeEnd()
    {
        mRuntimeScene->OnRuntimeEnd();
        mRuntimeScene->SetRunning(false);
        mRuntimeScene.Reset();

        // The runtime scene removed its canvases, this also drops roots set with SetUIRoot() from regular scripts so no UI leaks into edit mode
        mRenderer->GetUIManager().ClearCanvases();

        mPanelManager.GetPanel<SceneHierarchyPanel>()->SetSceneContext(mActiveScene.Raw());
        mPanelManager.GetPanel<ViewportPanel>()->OnSceneContextChanged();
        mShowAxes = true;
    }

    void Editor::LoadScene(Ref<Scene>&& scene)
    {
        mPanelManager.GetPanel<SceneHierarchyPanel>()->SetSceneContext(scene.Raw());
        mPanelManager.GetPanel<ViewportPanel>()->OnSceneContextChanged();
        mActiveScene = std::move(scene);
        mActiveScene->SetSelectedEntity({});
        mActiveScene->SetRenderDebug(true);
        mHistory.SetScene(mActiveScene.Raw());
        //mAssetImporter.ScanAndCookAll();
    }

    void Editor::CheckResize()
    {
        ViewportPanel* viewportPanel = mPanelManager.GetPanel<ViewportPanel>();
        Scope<GraphicsRHI>& rhi = mRenderer->GetRHI();

        glm::vec2 viewportSize = viewportPanel->GetViewportSize();
        FramebufferHandle fbHandle = mRenderer->GetFinalFramebuffer();
        FramebufferDesc desc = rhi->GetDesc(fbHandle);

        if (viewportSize.x > 0.0f && viewportSize.y > 0.0f && (desc.Width != (Uint)viewportSize.x || desc.Height != (Uint)viewportSize.y))
        {
            rhi->WaitIdle();

            mCamera.SetViewportSize(viewportSize);
            mRenderer->ForceResize((Uint)viewportSize.x, (Uint)viewportSize.y);

            if(mRuntimeScene && mRuntimeScene->GetMainCameraEntity().Data1)
                mRuntimeScene->OnResize(viewportSize.x, viewportSize.y);
            else
                mActiveScene->OnResize(viewportSize.x, viewportSize.y);
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // Scene editing
    //////////////////////////////////////////////////////////////////////////

    // "Cube" -> "Cube (1)", "Cube (1)" -> "Cube (2)"
    static String MakeDuplicateName(const String& name)
    {
        const size_t open = name.rfind(" (");
        if(open != String::npos && name.back() == ')' && name.size() > open + 3)
        {
            const String digits = name.substr(open + 2, name.size() - open - 3);
            if(digits.size() < 9 && std::all_of(digits.begin(), digits.end(), [](char c) { return c >= '0' && c <= '9'; }))
                return std::format("{} ({})", name.substr(0, open), std::stoi(digits) + 1);
        }
        return std::format("{} (1)", name);
    }

    Entity Editor::GetSelectedEntity()
    {
        return mPanelManager.GetPanel<SceneHierarchyPanel>()->GetSelectedEntity();
    }

    void Editor::SelectEntity(Entity entity)
    {
        mPanelManager.GetPanel<SceneHierarchyPanel>()->SetSelectedEntity(entity);
        GetContextScene()->SetSelectedEntity(entity);
    }

    void Editor::SelectEntityByUUID(UUID id)
    {
        SelectEntity(SceneEditUtils::FindEntity(*mActiveScene.Raw(), id));
    }

    void Editor::OnEntityCreated(Entity entity)
    {
        if(!entity)
            return;

        if(!IsPlaying())
            mHistory.Push(CreateScope<EntityLifetimeCommand>(std::format("Create '{}'", SceneEditUtils::GetName(entity)), EntityLifetimeCommand::Kind::CREATED, EntityTreeSnapshot::Capture(entity)));
        SelectEntity(entity);
    }

    void Editor::DeleteEntity(Entity entity)
    {
        if(!entity)
            return;

        if(!IsPlaying())
            mHistory.Push(CreateScope<EntityLifetimeCommand>(std::format("Delete '{}'", SceneEditUtils::GetName(entity)), EntityLifetimeCommand::Kind::DELETED, EntityTreeSnapshot::Capture(entity)));

        Entity selected = GetSelectedEntity();
        if(selected && (selected == entity || SceneEditUtils::IsDescendantOf(selected, entity)))
            SelectEntity({});

        entity.GetScene()->DestroyEntity(entity);
    }

    Entity Editor::DuplicateEntity(Entity entity)
    {
        if(!entity || entity.GetScene() != GetContextScene())
            return {};

        // Running scene: the engine path copies whole components so the physics/script hooks see complete data (root entities only)
        if(IsPlaying())
        {
            Entity clone = entity.GetScene()->DuplicateEntity(entity);
            if(clone)
                SelectEntity(clone);
            return clone;
        }

        Entity clone = EntityTreeSnapshot::Capture(entity).Instantiate(*mActiveScene.Raw(), SceneEditUtils::GetParent(entity), entity);
        String& cloneName = clone.GetComponent<NameComponent>().Name;
        cloneName = MakeDuplicateName(cloneName);

        mHistory.Push(CreateScope<EntityLifetimeCommand>(std::format("Duplicate '{}'", SceneEditUtils::GetName(entity)), EntityLifetimeCommand::Kind::CREATED, EntityTreeSnapshot::Capture(clone)));
        SelectEntity(clone);
        RefreshCameraViewports();
        return clone;
    }

    void Editor::CopyEntity(Entity entity)
    {
        if(!entity)
            return;

        // Works while playing too: copy a tweaked runtime entity, stop, paste it into the scene
        mClipboard = EntityTreeSnapshot::Capture(entity);
        Log<Severity::Info>("[Editor] Copied '{}'", SceneEditUtils::GetName(entity));
    }

    void Editor::PasteEntity()
    {
        if(!mClipboard.IsValid() || IsPlaying())
            return;

        Scene& scene = *mActiveScene.Raw();
        Entity selected = GetSelectedEntity();
        if(selected && selected.GetScene() != &scene)
            selected = {};

        Entity clone = mClipboard.Instantiate(scene, selected ? SceneEditUtils::GetParent(selected) : Entity {}, selected);
        mHistory.Push(CreateScope<EntityLifetimeCommand>(std::format("Paste '{}'", SceneEditUtils::GetName(clone)), EntityLifetimeCommand::Kind::CREATED, EntityTreeSnapshot::Capture(clone)));
        SelectEntity(clone);
        RefreshCameraViewports();
    }

    void Editor::ReparentEntity(Entity entity, Entity newParent)
    {
        if(!entity)
            return;

        Entity oldParent = SceneEditUtils::GetParent(entity);
        if(oldParent == newParent)
            return;

        if(newParent && (newParent == entity || SceneEditUtils::IsDescendantOf(newParent, entity)))
        {
            Log<Severity::Warn>("[Editor] Cannot parent an entity to its own descendant!");
            return;
        }

        const ReparentCommand::Placement from = { SceneEditUtils::GetUUID(oldParent), SceneEditUtils::GetUUID(SceneEditUtils::GetPreviousSibling(entity)) };
        entity.GetScene()->SetParent(entity, newParent);
        if(entity.HasComponent<TransformComponent>())
            entity.GetComponent<TransformComponent>().MarkDirty(); // World transform depends on the parent
        const ReparentCommand::Placement to = { SceneEditUtils::GetUUID(newParent), SceneEditUtils::GetUUID(SceneEditUtils::GetPreviousSibling(entity)) };

        if(!IsPlaying())
        {
            String name = newParent ? std::format("Parent '{}' to '{}'", SceneEditUtils::GetName(entity), SceneEditUtils::GetName(newParent))
                                    : std::format("Unparent '{}'", SceneEditUtils::GetName(entity));
            mHistory.Push(CreateScope<ReparentCommand>(std::move(name), SceneEditUtils::GetUUID(entity), from, to));
        }
    }

    void Editor::SaveScene()
    {
        const AssetID sceneID = mActiveScene->GetID();
        if(!sceneID || !mAssetManager->GetMetadata(sceneID).IsValid())
        {
            Log<Severity::Warn>("[Editor] The current scene is not a registered asset, nothing to save");
            return;
        }

        mHistory.FlushTracked(); // So the "saved" marker lands after an edit that is still being tracked
        mAssetManager->Save(sceneID);
        mHistory.MarkSaved();
        Log<Severity::Info>("[Editor] Saved scene: {}", mAssetManager->GetMetadata(sceneID).RelativePath);
    }

    void Editor::RequestUndo()
    {
        QueueHistoryStep([](EditorHistory& history, UUID& selection) -> bool { return history.Undo(selection); });
    }

    void Editor::RequestRedo()
    {
        QueueHistoryStep([](EditorHistory& history, UUID& selection) -> bool { return history.Redo(selection); });
    }

    void Editor::RequestHistoryJump(size_t position)
    {
        QueueHistoryStep([position](EditorHistory& history, UUID& selection) -> bool { return history.JumpTo(position, selection); });
    }

    void Editor::QueueHistoryStep(std::function<bool(EditorHistory&, UUID&)> step)
    {
        if(IsPlaying())
            return;

        Core::AddFrameEndCallback([this, step = std::move(step)]() {
            UUID selection = UUID::INVALID;
            if(IsPlaying() || !step(mHistory, selection))
                return;

            SelectEntityByUUID(selection);
            RefreshCameraViewports();
        });
    }

    void Editor::RefreshCameraViewports()
    {
        // RuntimeCamera's viewport size is not serialized, cameras recreated from a snapshot need it again
        const glm::vec2& viewportSize = mViewportPanel->GetViewportSize();
        mActiveScene->OnResize(viewportSize.x, viewportSize.y);
    }

    void Editor::ConfirmDiscardChanges(const char* action, std::function<void()> onConfirm)
    {
        if(!mCurrentProject.IsValid() || !mHistory.IsDirty())
        {
            onConfirm();
            return;
        }

        // History is dirty, show a confirmation dialog before discarding changes
        const String message = std::format("The scene has unsaved changes, they will be lost.\n{} without saving?", action);
        ImGuiAux::ShowConfirmationBox("Unsaved Changes", message, std::move(onConfirm));
    }

    void Editor::HandleShortcuts()
    {
        const ImGuiIO& io = ImGui::GetIO();
        const bool ctrl = io.KeyCtrl;
        const bool shift = io.KeyShift;

        // RMB + WASDQE (+Ctrl/Shift) flies the camera, none of these may fire then
        if(ImGui::IsMouseDown(ImGuiMouseButton_Right))
            return;

        // Before the text input check: Ctrl+S right after typing a value must still save
        if(ctrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
            SaveScene();

        // Text fields keep their own Ctrl+Z/C/V, the UI Editor runs its own history while it is focused/hovered
        if(io.WantTextInput || mPanelManager.GetPanel<UIEditorPanel>()->OwnsShortcuts())
            return;

        if(ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_Z))
            RequestUndo();
        if(ctrl && (ImGui::IsKeyPressed(ImGuiKey_Y) || (shift && ImGui::IsKeyPressed(ImGuiKey_Z))))
            RequestRedo();

        // Entity shortcuts only while the Hierarchy or the Viewport has the user's attention, other panels reuse these keys (Content Browser: Ctrl+C)
        SceneHierarchyPanel* hierarchy = mPanelManager.GetPanel<SceneHierarchyPanel>();
        if(!hierarchy->IsFocusedOrHovered() && !mViewportPanel->IsFocusedOrHovered())
            return;

        Entity selected = GetSelectedEntity();
        if(ctrl && ImGui::IsKeyPressed(ImGuiKey_C, false) && selected)
            CopyEntity(selected);
        if(ctrl && ImGui::IsKeyPressed(ImGuiKey_V, false))
            PasteEntity();
        if(ctrl && ImGui::IsKeyPressed(ImGuiKey_D, false) && selected)
            DuplicateEntity(selected);
        if(ImGui::IsKeyPressed(ImGuiKey_Delete, false) && selected)
            DeleteEntity(selected);
    }

    void Editor::DrawEditMenu()
    {
        if(!ImGui::BeginPopup("EditMenu", ImGuiPopupFlags_None))
            return;

        {
            ImGuiAux::ScopedBoldFont font;
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "EDIT");
        }
        ImGuiAux::StyledSeparator();

        const bool playing = IsPlaying();
        const String undoLabel = mHistory.CanUndo() ? std::format("Undo {}", mHistory.GetUndoName()) : String("Undo");
        const String redoLabel = mHistory.CanRedo() ? std::format("Redo {}", mHistory.GetRedoName()) : String("Redo");

        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.18f, 0.18f, 0.18f, 1.0f));
        if(ImGuiAux::StyledMenuItem(undoLabel.c_str(), "Ctrl+Z", nullptr, !playing && mHistory.CanUndo()))
            RequestUndo();
        if(ImGuiAux::StyledMenuItem(redoLabel.c_str(), "Ctrl+Y", nullptr, !playing && mHistory.CanRedo()))
            RequestRedo();

        ImGuiAux::StyledSeparator();

        Entity selected = GetSelectedEntity();
        const bool hasSelection = (bool)selected;
        if(ImGuiAux::StyledMenuItem("Copy", "Ctrl+C", nullptr, hasSelection))
            CopyEntity(selected);
        if(ImGuiAux::StyledMenuItem("Paste", "Ctrl+V", nullptr, !playing && HasEntityInClipboard()))
            PasteEntity();
        if(ImGuiAux::StyledMenuItem("Duplicate", "Ctrl+D", nullptr, hasSelection))
            DuplicateEntity(selected);
        if(ImGuiAux::StyledMenuItem("Delete", "Del", nullptr, hasSelection))
            DeleteEntity(selected);

        ImGuiAux::StyledSeparator();

        if(ImGuiAux::StyledMenuItem("Save Scene", "Ctrl+S"))
            SaveScene();
        ImGui::PopStyleColor();

        ImGui::EndPopup();
    }

    void Editor::DrawConsoleCounters(float rightEdgeX, float y)
    {
        EditorConsole& console = EditorConsole::Get();
        Uint warnings = 0, errors = 0;
        {
            std::lock_guard<std::mutex> lock(console.GetMutex());
            warnings = console.GetWarningCount();
            errors = console.GetErrorCount();
        }

        char warningText[32], errorText[32];
        std::snprintf(warningText, sizeof(warningText), "%u WARN", warnings);
        std::snprintf(errorText, sizeof(errorText), "%u ERR", errors);

        constexpr float spacing = 10.0f;
        const float width = ImGui::CalcTextSize(warningText).x + spacing + ImGui::CalcTextSize(errorText).x;
        ImGui::SetCursorPos(ImVec2(rightEdgeX - width, y));

        auto counter = [this](const char* text, Uint count, const ImVec4& color) {
            ImGui::PushStyleColor(ImGuiCol_Text, count > 0 ? color : ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
            ImGui::TextUnformatted(text);
            ImGui::PopStyleColor();
            if(ImGui::IsItemHovered())
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            if(ImGui::IsItemClicked())
                mPanelManager.GetPanel<ConsolePanel>()->Focus();
        };

        counter(warningText, warnings, ImVec4(1.0f, 0.8f, 0.25f, 1.0f));
        ImGui::SameLine(0.0f, spacing);
        counter(errorText, errors, ImVec4(1.0f, 0.4, 0.45f, 1.0f));
    }

    void Editor::RenderShortcutsWindow()
    {
        if(!mShowShortcutsWindow)
            return;

        struct Row
        {
            const char* Keys;
            const char* Action; // nullptr: section title
        };

        static constexpr Row sRows[] = {
            { "GENERAL", nullptr },
            { "Ctrl+S", "Save scene" },
            { "Ctrl+Z", "Undo" },
            { "Ctrl+Y / Ctrl+Shift+Z", "Redo" },
            { "F5", "Play / Stop" },
            { "ENTITIES (Hierarchy or Viewport focused)", nullptr },
            { "Ctrl+C", "Copy entity (with children)" },
            { "Ctrl+V", "Paste after the selection" },
            { "Ctrl+D", "Duplicate" },
            { "Delete", "Delete" },
            { "F", "Focus the camera on the selection" },
            { "VIEWPORT", nullptr },
            { "RMB + WASD / Q E", "Fly the camera" },
            { "Q / W / E / R / T", "Gizmo: none / move / rotate / scale / universal" },
            { "Ctrl (hold)", "Snap while using the gizmo" },
            { "F11 / Esc", "Fullscreen viewport / leave fullscreen" },
            { "UI EDITOR (focused)", nullptr },
            { "Ctrl+Z / Ctrl+Y", "Undo / Redo layout edits" },
            { "Ctrl+S", "Save layout" },
            { "Ctrl+D / Delete", "Duplicate / delete widget" },
            { "Arrows (+Shift)", "Nudge 1 (10) units" },
            { "CONTENT BROWSER", nullptr },
            { "F2", "Rename" },
            { "Ctrl+C", "Copy the asset path" },
        };

        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if(ImGui::Begin("Keyboard Shortcuts", &mShowShortcutsWindow, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize))
        {
            if(ImGui::BeginTable("##Shortcuts", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
            {
                for(const Row& row : sRows)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGuiAux::ScopedBoldFont font;
                    if(!row.Action)
                    {
                        ImGui::Dummy(ImVec2(0.0f, 4.0f));
                        ImGui::TextColored(ImGuiAux::Colors::ThemeColor1, "%s", row.Keys);
                        continue;
                    }

                    ImGui::TextUnformatted(row.Keys);
                    ImGui::TableNextColumn();
                    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // Regular
                    ImGui::TextUnformatted(row.Action);
                    ImGui::PopFont();
                }
                ImGui::EndTable();
            }
        }
        ImGui::End();
    }

    void Editor::OnShutdown()
    {
        mAssetImporter.Shutdown();
        Scope<GraphicsRHI>& rhi = Core::GetRenderer()->GetRHI();
        rhi->DestroyImage(mEngineLogo);
        rhi->DestroyImage(mMinimize);
        rhi->DestroyImage(mMaximize);
        rhi->DestroyImage(mClose);
    }

} // namespace Surge

// Entry point
int main()
{
    Surge::ClientOptions clientOptions;
    clientOptions.EnableImGui = true;
    clientOptions.RenderFinalImageToSwapchian = false; // We grab the imgui image id from renderer
    clientOptions.WindowDescription = {1280, 720, "Surge Editor", Surge::WindowFlags::NO_TITLEBAR};

    Surge::EditorConsole::Install(); // Before Core::Initialize so the Console also shows the engine startup logs

    Surge::Editor* app = Surge::MakeClient<Surge::Editor>();
    app->SetOptions(clientOptions);

    Surge::Core::Initialize(app);
    Surge::Core::Run();
    Surge::Core::Shutdown();

    Surge::EditorConsole::Uninstall();
}