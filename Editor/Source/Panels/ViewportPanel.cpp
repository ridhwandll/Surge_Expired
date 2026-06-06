// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Panels/ViewportPanel.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Core/Input/Input.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/ECS/Components.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "SurgeMath/Math.hpp"
#include "Editor.hpp"
#include "ContentBrowserPanel.hpp"

#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>

namespace Surge
{
    void ViewportPanel::Init(void* panelInitArgs)
    {
        mCode = GetStaticCode();
        mSceneHierarchy = static_cast<Editor*>(Core::GetClient())->GetPanelManager().GetPanel<SceneHierarchyPanel>();
        mEditorCam = static_cast<EditorCamera*>(panelInitArgs);
        SetSceneName();

        ImGuizmo::Style& style = ImGuizmo::GetStyle();

        style.CenterCircleSize = 4.0f;
        style.TranslationLineThickness = 5.0f;
        style.TranslationLineArrowSize = 9.0f;
        //style.RotationLineThickness = 3.0f;
        //style.RotationOuterLineThickness = 5.0f;
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
        dispatcher.Dispatch<KeyPressedEvent>([&](KeyPressedEvent& keyEvent) -> bool {

            // Hit F to Focus
            if (keyEvent.GetKeyCode() == Key::F)
            {
                const Entity& selectedEntity = mSceneHierarchy->GetSelectedEntity();
                if (selectedEntity)
                {
                    const TransformComponent& transform = selectedEntity.GetComponent<TransformComponent>();
                    mEditorCam->Focus(transform.Position);
                }
            }

            if (!Input::IsMouseButtonPressed(Mouse::ButtonRight))
            {
                switch (keyEvent.GetKeyCode())
                {
                    // Gizmos
                    case Key::Q:
                    {
                        if (!mGizmoInUse)
                            mGizmoType = -1;
                        break;
                    }
                    case Key::W:
                    {
                        if (!mGizmoInUse)
                            mGizmoType = ImGuizmo::OPERATION::TRANSLATE;
                        break;
                    }
                    case Key::E:
                    {
                        if (!mGizmoInUse)
                            mGizmoType = ImGuizmo::OPERATION::ROTATE;
                        break;
                    }
                    case Key::R:
                    {
                        if (!mGizmoInUse)
                            mGizmoType = ImGuizmo::OPERATION::SCALE;
                        break;
                    }
                    case Key::T:
                    {
                        if (!mGizmoInUse)
                            mGizmoType = ImGuizmo::OPERATION::UNIVERSAL;
                        break;
                    }
                }
            }
            return false;
        });
    }

    void ViewportPanel::Render(bool* show)
    {
        if(!*show)
            return;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

        if(ImGui::Begin(PanelCodeToString(mCode), show, windowFlags))
        {
            mIsViewportHovered = ImGui::IsWindowHovered();
            mViewportSize = { ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y };
            ImTextureID mTexID = Core::GetRenderer()->GetFinalImageImGuiID();

            ImVec2 cursorLocalPos = ImGui::GetCursorPos();
            ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();

            ImGui::Image(mTexID, { mViewportSize.x, mViewportSize.y });

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
                                newEntity.AddComponent<MeshComponent>().MeshID = droppedAssetID;
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

            ImFont* boldFont = ImGui::GetIO().Fonts->Fonts[1];

            // Scene Name
            ImGui::PushFont(boldFont);
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            float padding = 5.0f;
            float textHeight = ImGui::GetTextLineHeight();
            float textWidth = ImGui::CalcTextSize(mSceneName.c_str()).x;
            ImVec2 bgMax = ImVec2(cursorScreenPos.x + mViewportSize.x - 10.0f, cursorScreenPos.y + 10.0f + textHeight + (padding * 2.0f));
            ImVec2 bgMin = ImVec2(bgMax.x - textWidth - (padding * 2.0f), cursorScreenPos.y + 10.0f);
            drawList->AddRectFilled(bgMin, bgMax, IM_COL32(15, 15, 15, 200), 4.0f);
            drawList->AddText(ImVec2(bgMin.x + padding, bgMin.y + padding), IM_COL32(230, 230, 230, 255), mSceneName.c_str());
            ImGui::PopFont();

            // Button overlays
            float buttonWidth = 50.0f;
            float buttonSpacing = ImGui::GetStyle().ItemSpacing.x;
            float totalButtonsWidth = (buttonWidth * 2.0f) + buttonSpacing;
            ImVec2 buttonsPos = ImVec2(cursorLocalPos.x + (mViewportSize.x - totalButtonsWidth) * 0.5f, cursorLocalPos.y + 10.0f);
            ImGui::SetCursorPos(buttonsPos);

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.1f, 0.1f, 0.6f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

            if(ImGui::Button("PLAY", ImVec2(buttonWidth, 0))) { Log<Severity::Warn>("[ViewportPanel] TODO: Implement Play button"); }
            ImGui::SameLine();
            if(ImGui::Button("PAUSE", ImVec2(buttonWidth, 0))) { Log<Severity::Warn>("[ViewportPanel] TODO: Implement Pause button"); }

            ImGui::PopStyleVar();
            ImGui::PopStyleColor();

            // GIZMOS
            Entity& selectedEntity = mSceneHierarchy->GetSelectedEntity();
            if(selectedEntity && mGizmoType > 0)
            {
                ImGuizmo::SetOrthographic(false);
                ImGuizmo::SetDrawlist();

                ImGuizmo::SetRect(cursorScreenPos.x, cursorScreenPos.y, mViewportSize.x, mViewportSize.y);

                glm::mat4 cameraView, cameraProjection;
                Editor* app = static_cast<Editor*>(Core::GetClient());
                EditorCamera& camera = app->GetCamera();
                cameraProjection = camera.GetProjectionMatrix();
                cameraProjection[1][1] *= -1; // Vulkan flip
                cameraView = camera.GetViewMatrix();

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

                    glm::vec3 translation, rotation, scale;
                    Math::DecomposeTransform(transform, translation, rotation, scale);

                    glm::vec3 deltaRotation = glm::degrees(rotation) - transformComponent.Rotation;
                    transformComponent.Position = translation;
                    transformComponent.Rotation += deltaRotation;
                    transformComponent.Scale = scale;
                }
                else
                    mGizmoInUse = false;
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

    void ViewportPanel::Shutdown()
    {
    }

    void ViewportPanel::SetSceneName()
    {
        auto assetID = mSceneHierarchy->GetSceneContext()->GetID();
        if (assetID)
            mSceneName = Filesystem::GetNameWithExtension(Core::GetAssetManager()->GetMetadata(assetID).RelativePath);
        else
            mSceneName = "Untitled Scene";
    }

} // namespace Surge