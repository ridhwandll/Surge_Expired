// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Panels/ViewportPanel.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Core/Input/Input.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/ECS/Components.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "SurgeMath/Math.hpp"
#include "Editor.hpp"
#include "ContentBrowserPanel.hpp"

#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>
#include "Surge/Graphics/UISystem/UIManager.hpp"
#include "Utility/ImGuiAux.hpp"

namespace Surge
{
    void ViewportPanel::Init(void* panelInitArgs)
    {
        mCode = GetStaticCode();
        mSceneHierarchy = static_cast<Editor*>(Core::GetClient())->GetPanelManager().GetPanel<SceneHierarchyPanel>();
        mEditorCam = static_cast<EditorCamera*>(panelInitArgs);

        ImGuizmo::Style& style = ImGuizmo::GetStyle();

        style.CenterCircleSize = 4.0f;
        style.TranslationLineThickness = 5.0f;
        style.TranslationLineArrowSize = 9.0f;
        style.RotationLineThickness = 4.0f;
        style.RotationOuterLineThickness = 5.0f;
        style.ScaleLineThickness = 5.0f;
        style.ScaleLineCircleSize = 9.0f;
        style.HatchedAxisLineThickness = 0.0f;

        style.Colors[ImGuizmo::COLOR::DIRECTION_X] = ImVec4(0.984f, 0.200f, 0.200f, 0.95f);
        style.Colors[ImGuizmo::COLOR::PLANE_X] = ImVec4(0.984f, 0.200f, 0.200f, 0.25f);

        style.Colors[ImGuizmo::COLOR::DIRECTION_Y] = ImVec4(0.553f, 0.831f, 0.000f, 0.95f);
        style.Colors[ImGuizmo::COLOR::PLANE_Y] = ImVec4(0.553f, 0.831f, 0.000f, 0.25f);

        style.Colors[ImGuizmo::COLOR::DIRECTION_Z] = ImVec4(0.137f, 0.565f, 1.000f, 0.95f);
        style.Colors[ImGuizmo::COLOR::PLANE_Z] = ImVec4(0.137f, 0.565f, 1.000f, 0.25f);

        style.Colors[ImGuizmo::COLOR::SELECTION] = ImVec4(1.000f, 0.659f, 0.000f, 1.00f);
        style.Colors[ImGuizmo::COLOR::TRANSLATION_LINE] = ImVec4(1.000f, 0.659f, 0.000f, 1.00f);
        style.Colors[ImGuizmo::COLOR::ROTATION_USING_BORDER] = ImVec4(1.000f, 0.659f, 0.000f, 1.00f);
        style.Colors[ImGuizmo::COLOR::ROTATION_USING_FILL] = ImVec4(1.000f, 0.659f, 0.000f, 1.00f);
    }

    void ViewportPanel::OnEvent(Event& e)
    {
        if(!mIsViewportHovered)
            return;

        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<KeyPressedEvent>([&](KeyPressedEvent& keyEvent) {

            // Hit F to Focus
            if(keyEvent.GetKeyCode() == Key::F)
            {
                const Entity& selectedEntity = mSceneHierarchy->GetSelectedEntity();
                if(selectedEntity)
                {
                    const TransformComponent& transform = selectedEntity.GetComponent<TransformComponent>();
                    mEditorCam->Focus(transform.Position);
                }
            }

            if(!Input::IsMouseButtonPressed(Mouse::ButtonRight))
            {
                switch(keyEvent.GetKeyCode())
                {
                    case Key::F11:
                    {
                        mIsFullscreen = !mIsFullscreen;
                        if(!mIsFullscreen)
                            mRestoreScreenPosBeforeFullscreen = true;

                        static_cast<Editor*>(Core::GetClient())->ShowTitlebar(!mIsFullscreen);
                        break;
                    }
                    case Key::Escape:
                    {
                        if(mIsFullscreen)
                        {
                            mIsFullscreen = false;
                            mRestoreScreenPosBeforeFullscreen = true;
                            static_cast<Editor*>(Core::GetClient())->ShowTitlebar(true);
                        }
                        break;
                    }

                    case Key::Q:
                    {
                        if(!mGizmoInUse)
                            mGizmoType = -1;
                        break;
                    }
                    case Key::W:
                    {
                        if(!mGizmoInUse)
                            mGizmoType = ImGuizmo::OPERATION::TRANSLATE;
                        break;
                    }
                    case Key::E:
                    {
                        if(!mGizmoInUse)
                            mGizmoType = ImGuizmo::OPERATION::ROTATE;
                        break;
                    }
                    case Key::R:
                    {
                        if(!mGizmoInUse)
                            mGizmoType = ImGuizmo::OPERATION::SCALE;
                        break;
                    }
                    case Key::T:
                    {
                        if(!mGizmoInUse)
                            mGizmoType = ImGuizmo::OPERATION::UNIVERSAL;
                        break;
                    }
                    default:
                        break;
                }
            }
        });
    }

    void ViewportPanel::Render(bool* show)
    {
        if(!*show)
            return;

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

        // Fullscreen handling
        if(mIsFullscreen)
        {
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->Pos);
            ImGui::SetNextWindowSize(viewport->Size);
            ImGui::SetNextWindowViewport(viewport->ID);
            windowFlags |= ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        }
        else if(mRestoreScreenPosBeforeFullscreen)
        {
            ImGui::SetNextWindowDockID(mPreviousDockID, ImGuiCond_Always);
            mRestoreScreenPosBeforeFullscreen = false;
            ImGui::SetWindowFocus(PanelCodeToString(mCode));
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });
        if(ImGui::Begin(PanelCodeToString(mCode), show, windowFlags))
        {
            Editor* editor = static_cast<Editor*>(Core::GetClient());
            bool isPlaying = editor->IsPlaying();

            // Aspect Ratio
            static int currentAspectMode = 0;
            constexpr const char* aspectRatios[] = { "Free Aspect", "16:9 Landscape", "9:16 Portrait", "21:9 Cinematic", "4:3 Classic" };

            ImVec2 availableSpace = ImGui::GetContentRegionAvail();
            float targetAspect = 0.0f;

            if(currentAspectMode == 1) targetAspect = 16.0f / 9.0f;
            else if(currentAspectMode == 2) targetAspect = 9.0f / 16.0f;
            else if(currentAspectMode == 3) targetAspect = 21.0f / 9.0f;
            else if(currentAspectMode == 4) targetAspect = 4.0f / 3.0f;

            float renderWidth = availableSpace.x;
            float renderHeight = availableSpace.y;

            if(targetAspect > 0.0f)
            {
                renderWidth = availableSpace.x;
                renderHeight = renderWidth / targetAspect;

                // If it exceeds the available height, fit height instead
                if(renderHeight > availableSpace.y)
                {
                    renderHeight = availableSpace.y;
                    renderWidth = renderHeight * targetAspect;
                }
            }
            float offsetX = (availableSpace.x - renderWidth) * 0.5f;
            float offsetY = (availableSpace.y - renderHeight) * 0.5f;

            mViewportSize = { renderWidth, renderHeight };
            mIsViewportHovered = ImGui::IsWindowHovered();

            if(!mIsFullscreen)
                mPreviousDockID = ImGui::GetWindowDockID();

            // ImGui Cursor setup
            ImVec2 cursorStartPos = ImGui::GetCursorPos();
            ImGui::SetCursorPos({ cursorStartPos.x + offsetX, cursorStartPos.y + offsetY });

            ImVec2 viewportBoundsMin = ImGui::GetCursorScreenPos();
            ImVec2 mainWindowPos = ImGui::GetMainViewport()->Pos;
            float uiBoundsX = viewportBoundsMin.x - mainWindowPos.x;
            float uiBoundsY = viewportBoundsMin.y - mainWindowPos.y;
            Core::GetRenderer()->GetUIManager().SetViewportBounds(uiBoundsX, uiBoundsY, renderWidth, renderHeight);

            if(!mIsFullscreen)
                mPreviousDockID = ImGui::GetWindowDockID();

            ImTextureID mTexID = Core::GetRenderer()->GetFinalImageImGuiID();
            ImGui::Image(mTexID, { renderWidth, renderHeight });

            if(ImGui::BeginDragDropTarget())
            {
                if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_PAYLOAD))
                {
                    SG_ASSERT(payload->DataSize == sizeof(AssetID), "Payload size mismatch!");
                    AssetID droppedAssetID = *(const AssetID*)payload->Data;
                    AssetManager* assetManager = Core::GetAssetManager();
                    const AssetMetadata& metadata = assetManager->GetMetadata(droppedAssetID);

                    switch(metadata.Type)
                    {
                        case AssetType::SCENE:
                        {
                            Ref<Scene> droppedScene = assetManager->Load<Scene>(droppedAssetID);
                            if(droppedScene)
                            {
                                auto* editor = static_cast<Editor*>(Core::GetClient());
                                editor->LoadScene(std::move(droppedScene));
                                SetSceneName();
                            }
                            break;
                        }
                        case AssetType::MESH:
                        {
                            Ref<Mesh> droppedMesh = assetManager->Load<Mesh>(droppedAssetID);
                            if(droppedMesh)
                            {
                                auto* editor = static_cast<Editor*>(Core::GetClient());
                                Ref<Scene> currentScene = editor->GetCurrentScene();
                                Entity newEntity;
                                currentScene->CreateEntity(newEntity, "Mesh");
                                newEntity.AddComponent<MeshComponent>().MeshAsset = droppedMesh;
                                mSceneHierarchy->SetSelectedEntity(newEntity);
                            }
                            break;
                        }
                        default:
                            Log<Severity::Warn>("[ViewportPanel] Failed to load dropped asset in viewport!");
                            break;
                    }
                }
                ImGui::EndDragDropTarget();
            }

            // Overlays
            ImFont* boldFont = ImGui::GetIO().Fonts->Fonts[1];
            float padding = 3.0f;

            // Aspect Ratio Dropdown
            if(!isPlaying)
            {
                ImGui::SetCursorPos(ImVec2(cursorStartPos.x + offsetX + padding, cursorStartPos.y + offsetY + padding));
                ImGui::SetNextItemWidth(140.0f);
                ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(15, 15, 15, 200));
                if(ImGui::BeginCombo("##AspectCombo", aspectRatios[currentAspectMode]))
                {
                    for(int i = 0; i < 5; i++)
                    {
                        bool isSelected = (currentAspectMode == i);
                        if(ImGui::Selectable(aspectRatios[i], isSelected))
                            currentAspectMode = i;
                        if(isSelected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopStyleColor();
            }

            // Scene Name
            const char* displayText = isPlaying ? "RUNTIME" : mSceneName.c_str();
            ImGui::PushFont(boldFont);
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            float textHeight = ImGui::GetTextLineHeight();
            float textWidth = ImGui::CalcTextSize(displayText).x;

            ImVec2 bgMax = ImVec2(viewportBoundsMin.x + renderWidth - padding, viewportBoundsMin.y + padding + textHeight + (padding * 2.0f));
            ImVec2 bgMin = ImVec2(bgMax.x - textWidth - (padding * 2.0f), viewportBoundsMin.y + padding);

            drawList->AddRectFilled(bgMin, bgMax, IM_COL32(15, 15, 15, 200), 4.0f);
            drawList->AddText(ImVec2(bgMin.x + padding, bgMin.y + padding), IM_COL32(230, 230, 230, 255), displayText);
            ImGui::PopFont();

            // Gizmos
            if(!isPlaying)
            {
                Entity& selectedEntity = mSceneHierarchy->GetSelectedEntity();
                Editor* app = static_cast<Editor*>(Core::GetClient());

                const EditorCamera& camera = app->GetCamera();
                glm::mat4 cameraView = camera.GetViewMatrix();
                glm::mat4 cameraProjection = camera.GetProjectionMatrix();
                cameraProjection[1][1] *= -1;

                // Set Gizmo bounds tightly to the pillarboxed area!
                ImGuizmo::SetRect(viewportBoundsMin.x, viewportBoundsMin.y, renderWidth, renderHeight);

                if(selectedEntity && mGizmoType > 0)
                {
                    ImGuizmo::SetDrawlist();

                    TransformComponent& transformComponent = selectedEntity.GetComponent<TransformComponent>();
                    glm::mat4 transform = transformComponent.GetTransform();

                    // Snapping
                    const bool snap = Input::IsKeyPressed(Key::LeftControl);
                    float snapValue = 0.5f;
                    if(mGizmoType == ImGuizmo::OPERATION::ROTATE)
                        snapValue = 45.0f;

                    float snapValues[3] = { snapValue, snapValue, snapValue };
                    ImGuizmo::SetGizmoSizeClipSpace(0.15f);
                    ImGuizmo::Manipulate(glm::value_ptr(cameraView), glm::value_ptr(cameraProjection), static_cast<ImGuizmo::OPERATION>(mGizmoType), ImGuizmo::LOCAL, glm::value_ptr(transform), nullptr, snap ? snapValues : nullptr);

                    if(ImGuizmo::IsUsing())
                    {
                        mGizmoInUse = true;

                        RelationshipComponent& rel = selectedEntity.GetComponent<RelationshipComponent>();

                        if(rel.Parent != entt::null)
                        {
                            Entity parentEntity((entt::entity)rel.Parent, app->GetCurrentScene().Raw());
                            glm::mat4 parentWorld = parentEntity.GetComponent<TransformComponent>().GetTransform();

                            transform = glm::inverse(parentWorld) * transform;
                        }

                        glm::vec3 translation, rotation, scale;
                        Math::DecomposeTransform(transform, translation, rotation, scale);

                        glm::vec3 deltaRotation = glm::degrees(rotation) - transformComponent.Rotation;
                        transformComponent.Position = translation;
                        transformComponent.Rotation += deltaRotation;
                        transformComponent.Scale = scale;
                        transformComponent.MarkDirty();
                    }
                    else
                        mGizmoInUse = false;
                }
            }
        }
        else
        {
            mIsViewportHovered = false;
            mViewportSize = { 0.0f, 0.0f };
        }

        ImGui::End();
        ImGui::PopStyleVar();
    }

    void ViewportPanel::Shutdown() {}

    void ViewportPanel::SetSceneName()
    {
        Scene* scene = mSceneHierarchy->GetSceneContext();
        AssetID assetID = scene ? scene->GetID() : AssetID(AssetID::INVALID);
        if(assetID)
            mSceneName = Filesystem::GetFilenameWithExt(Core::GetAssetManager()->GetMetadata(assetID).RelativePath);
        else
            mSceneName = "Untitled Scene";
    }

} // namespace Surge