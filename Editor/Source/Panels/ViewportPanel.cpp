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

                        ImGui::SetWindowFocus(PanelCodeToString(mCode));
                        break;
                    }
                    case Key::Escape:
                    {
                        if(mIsFullscreen)
                        {
                            mIsFullscreen = false;
                            mRestoreScreenPosBeforeFullscreen = true;
                            ImGui::SetWindowFocus(PanelCodeToString(mCode));
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
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });
        if(ImGui::Begin(PanelCodeToString(mCode), show, windowFlags))
        {
            if(!mIsFullscreen)
                mPreviousDockID = ImGui::GetWindowDockID();

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

            Editor* editor = static_cast<Editor*>(Core::GetClient());
            bool isPlaying = editor->IsPlaying();

            // Scene Name
            const char* displayText = isPlaying ? "RUNTIME" : mSceneName.c_str();
            ImGui::PushFont(boldFont);
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            float padding = 5.0f;
            float textHeight = ImGui::GetTextLineHeight();
            float textWidth = ImGui::CalcTextSize(displayText).x;
            ImVec2 bgMax = ImVec2(cursorScreenPos.x + mViewportSize.x - 10.0f, cursorScreenPos.y + 10.0f + textHeight + (padding * 2.0f));
            ImVec2 bgMin = ImVec2(bgMax.x - textWidth - (padding * 2.0f), cursorScreenPos.y + 10.0f);
            drawList->AddRectFilled(bgMin, bgMax, IM_COL32(15, 15, 15, 200), 4.0f);
            drawList->AddText(ImVec2(bgMin.x + padding, bgMin.y + padding), IM_COL32(230, 230, 230, 255), displayText);

            // Play Button
            const float buttonWidth = ImGui::CalcTextSize("STOP").x + 10.0f; //OR PLAY, both 4 chars
            float totalButtonsWidth = buttonWidth;
            ImVec2 buttonsPos = ImVec2(cursorLocalPos.x + (mViewportSize.x - totalButtonsWidth) * 0.5f, cursorLocalPos.y + 10.0f);
            ImGui::SetCursorPos(buttonsPos);

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.1f, 0.1f, 0.5f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
            if(isPlaying)
            {
                if(ImGui::Button("STOP", ImVec2(buttonWidth, 0)))
                    editor->OnRuntimeEnd();
            }
            else
            {
                if(ImGui::Button("PLAY", ImVec2(buttonWidth, 0)))
                    editor->OnRuntimeStart();
            }

            ImGui::PopFont();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();

            // GIZMOS
            Entity& selectedEntity = mSceneHierarchy->GetSelectedEntity();
            Editor* app = static_cast<Editor*>(Core::GetClient());

            const EditorCamera& camera = app->GetCamera();
            glm::mat4 cameraView = camera.GetViewMatrix();
            glm::mat4 cameraProjection = camera.GetProjectionMatrix();
            cameraProjection[1][1] *= -1;
            ImGuizmo::SetRect(cursorScreenPos.x, cursorScreenPos.y, mViewportSize.x, mViewportSize.y);
            //ImGuizmo::DrawGrid(glm::value_ptr(cameraView), glm::value_ptr(cameraProjection), glm::value_ptr(glm::mat4(1.0f)), 16);

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

                         // Local = Inverse(ParentWorld) * World
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