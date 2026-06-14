// Copyright (c) - SurgeTechnologies - All rights reserved
#include <Surge/Surge.hpp>
#include "Editor.hpp"
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
#include "Surge/Physics/Physics.hpp"

namespace Surge
{
    void Editor::OnInitialize()
    {
        // Dummy Scene to render the project browser ImGui. We can probably find a better way to do this later, but for now it works and doesn't cause any issues
        mActiveScene = Ref<Scene>::Create();

        mAssetManager = Core::GetAssetManager();
        mRenderer = Core::GetRenderer();
        mRenderer->SetOutlineThickness(1);

        mCamera = EditorCamera(45.0f, 1.778f, 0.1f, 1000.0f);
        mCamera.SetActive(true);

        // Configure panels
        SceneHierarchyPanel* sceneHierarchy = mPanelManager.PushPanel<SceneHierarchyPanel>();
        mPanelManager.PushPanel<InspectorPanel>()->SetHierarchy(sceneHierarchy);
        mViewportPanel = mPanelManager.PushPanel<ViewportPanel>(&mCamera);
        mPanelManager.PushPanel<ContentBrowserPanel>();
        mPanelManager.PushPanel<MaterialEditorPanel>();
        mPanelManager.PushPanel<ExportPanel>();

        mProjectBrowser.Init();

        mAssetImporter.Initialize(mAssetManager);
        mAssetImporter.RegisterCooker(CreateScope<Texture2DCooker>());
        mAssetImporter.RegisterCooker(CreateScope<MaterialCooker>());
        mAssetImporter.RegisterCooker(CreateScope<MeshCooker>());


        mAssetManager->AddAssetLoadHook([this](AssetID id, const AssetMetadata& meta)
                                        {
                                            if(mAssetImporter.NeedsCook(id, meta.Type))
                                            {
                                                Log<Severity::Warn>("[AssetManager::AddAssetLoadHook] Cooked file missing on Load, cooking now: {}", meta.RelativePath);
                                                mAssetImporter.RecookAsset(id);
                                            }
                                        });

        mRenderer->AddImGuiRenderCallback([this]() { OnImGuiRender(); });
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
                mCamera.OnUpdate(mViewportPanel->IsViewportHovered());
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
        if(mCurrentProject.IsValid())
        {
            mRenderer->ShowInternalImGui(true);
            ImGuiAux::DockSpace();
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
            mRenderer->ShowInternalImGui(false);
            mProjectBrowser.Render();
        }
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
                                                 if(keyEvent.GetKeyCode() == Key::F2)
                                                 {
                                                     mAssetManager->Save(mActiveScene->GetID());
                                                 }
                                             });
    }

    void Editor::OnRuntimeStart()
    {
        mRuntimeScene = Ref<Scene>::Create();
        mPanelManager.GetPanel<SceneHierarchyPanel>()->SetSceneContext(mRuntimeScene.Raw());
        mPanelManager.GetPanel<ViewportPanel>()->OnSceneContextChanged();

        mRuntimeScene->OnRuntimeStart();
        mActiveScene->CopyTo(mRuntimeScene.Raw());
        mRuntimeScene->SetRunning(true);

    }

    void Editor::OnRuntimeEnd()
    {
        mRuntimeScene->OnRuntimeEnd();
        mRuntimeScene->SetRunning(false);
        mRuntimeScene.Reset();

        mPanelManager.GetPanel<SceneHierarchyPanel>()->SetSceneContext(mActiveScene.Raw());
        mPanelManager.GetPanel<ViewportPanel>()->OnSceneContextChanged();
    }

    void Editor::LoadScene(Ref<Scene>&& scene)
    {
        mPanelManager.GetPanel<SceneHierarchyPanel>()->SetSceneContext(scene.Raw());
        mPanelManager.GetPanel<ViewportPanel>()->OnSceneContextChanged();
        mActiveScene = std::move(scene);
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

            mActiveScene->OnResize(viewportSize.x, viewportSize.y);
        }
    }

    void Editor::OnShutdown()
    {
        mAssetImporter.Shutdown();
    }

} // namespace Surge

// Entry point
int main()
{
    Surge::ClientOptions clientOptions;
    clientOptions.RenderFinalImageToSwapchian = false; // We grab the imgui image id from renderer
    clientOptions.WindowDescription = {1280, 720, "Surge Editor", Surge::WindowFlags::CreateDefault};

    Surge::Editor* app = Surge::MakeClient<Surge::Editor>();
    app->SetOptions(clientOptions);

    Surge::Core::Initialize(app);
    Surge::Core::Run();
    Surge::Core::Shutdown();
}