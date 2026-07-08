// Copyright (c) - SurgeTechnologies - All rights reserved
#include <Surge/Surge.hpp>
#include "Editor.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Core/Time/Clock.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"

#include "Utility/ImGuiAux.hpp"
#include "Panels/ViewportPanel.hpp"
#include "Panels/SceneHierarchyPanel.hpp"
#include "Panels/InspectorPanel.hpp"
#include "Panels/ContentBrowserPanel.hpp"
#include "Panels/MaterialEditorPanel.hpp"
#include "Panels/ExportPanel.hpp"

#include "Asset/Cookers/Texture2DCooker.hpp"
#include "Asset/Cookers/MeshCooker.hpp"
#include "Asset/Cookers/MaterialCooker.hpp"
#include "Asset/Cookers/ScriptCooker.hpp"
#include "Asset/Cookers/FontCooker.hpp"

#include "Surge/Physics/Physics.hpp"

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
                Log<Severity::Warn>("[EditorAssetLoadCallback] Cooked file outdated/missing on Load, cooking now: {}", meta.RelativePath);
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

        mAssetManager->AddAssetLoadCallback(CreateScope<EditorAssetLoadCallback>());

        // Configure panels
        SceneHierarchyPanel* sceneHierarchy = mPanelManager.PushPanel<SceneHierarchyPanel>();
        mPanelManager.PushPanel<InspectorPanel>()->SetHierarchy(sceneHierarchy);
        mViewportPanel = mPanelManager.PushPanel<ViewportPanel>(&mCamera);
        mPanelManager.PushPanel<ContentBrowserPanel>();
        mPanelManager.PushPanel<MaterialEditorPanel>();
        mPanelManager.PushPanel<ExportPanel>();

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

#ifdef SURGE_DEBUG
            DrawCustomTitlebar(("[DEBUG] Surge Editor //" + mCurrentProject.Name).c_str(), titleBarHeight);
#elif defined(SURGE_RELEASE)
            DrawCustomTitlebar(("Surge Editor //" + mCurrentProject.Name).c_str(), titleBarHeight);
#endif
            ImGuiAux::DockSpace(titleBarHeight);
            mPanelManager.RenderPanels();
            RenderEditorSettings();

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
            {
                mCurrentProject = {}; // Reset project to go back to the Project Browser
            }
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

                if(ImGui::Button("VIEW"))
                    ImGui::OpenPopup("WindowMenu");
                ImGui::SameLine();
                ImGui::Button("HELP");

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
                Platform::RequestExit();

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
                    float frameTimeMs = Core::GetClock().GetMilliseconds();
                    float fps = ImGui::GetIO().Framerate;
                    sFpsText = std::format("{:.2f} ms | {:.0f} FPS", frameTimeMs, fps);
                    sUpdateTimer = 0.0f;
                }

                float fpsWidth = ImGui::CalcTextSize(sFpsText.c_str()).x;

                ImGui::SetCursorPos(ImVec2(availableWidth - fpsWidth - rightPadding, 48.0f));
                ImGui::Text("%s", sFpsText.c_str());
            }
        }
        ImGui::End();

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
    }

    void Editor::RenderEditorSettings()
    {
        ImGui::Begin("Editor Settings");
        if (ImGuiAux::Button("Save Scene (F2)")) { mAssetManager->Save(mActiveScene->GetID()); }

        ImGui::Checkbox("Show Axes", &mShowAxes);
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
                                                 if(keyEvent.GetKeyCode() == Key::S)
                                                 {
                                                     bool ctrlPressed = Input::IsKeyPressed(Key::LeftControl);
                                                     if(ctrlPressed)
                                                     {
                                                         mAssetManager->Save(mActiveScene->GetID());
                                                         Log<Severity::Info>("[Editor] Saved scene: {}", mAssetManager->GetMetadata(mActiveScene->GetID()).RelativePath);
                                                     }
                                                 }
                                             });
    }

    void Editor::OnRuntimeStart()
    {
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

    Surge::Editor* app = Surge::MakeClient<Surge::Editor>();
    app->SetOptions(clientOptions);

    Surge::Core::Initialize(app);
    Surge::Core::Run();
    Surge::Core::Shutdown();
}