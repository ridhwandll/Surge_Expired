// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Panels/ViewportPanel.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Core/Input/Input.hpp"
#include "Surge/Core/Hash.hpp"
#include "Surge/ECS/Components.hpp"
#include "SurgeReflect/TypeTraits.hpp"
#include "SurgeMath/Math.hpp"
#include "Utility/ImGUIAux.hpp"
#include "Editor.hpp"
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
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<KeyPressedEvent>([&](KeyPressedEvent& keyEvent) -> bool {

            // Hit F to Focus
            if (keyEvent.GetKeyCode() == Key::F)
            {
                const Entity& selectedEntity = mSceneHierarchy->GetSelectedEntity();
                if (selectedEntity && mIsViewportHovered)
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
                }
            }
            return false;
        });
    }

    void ViewportPanel::Render(bool* show)
    {
        if (!*show)
            return;

         ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
         if (ImGui::Begin(PanelCodeToString(mCode), show))
         {
             mIsViewportHovered = ImGui::IsWindowHovered();
             mViewportSize = { ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y };
             ImTextureID mTexID = Core::GetRenderer()->GetFinalImageImGuiID();
             ImGui::Image(mTexID, { mViewportSize.x, mViewportSize.y });
         }
         else
         {
             mIsViewportHovered = false;
             mViewportSize = { 0.0f, 0.0f };
         }
 
            // Entity transform
         Entity& selectedEntity = mSceneHierarchy->GetSelectedEntity();
         if(selectedEntity && mGizmoType > 0)
         {
             ImGuizmo::SetOrthographic(false);
             ImGuizmo::SetDrawlist();
             ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, mViewportSize.x, mViewportSize.y);

             glm::mat4 cameraView, cameraProjection;
             Editor* app = static_cast<Editor*>(Core::GetClient());
             EditorCamera& camera = app->GetCamera();
             cameraProjection = camera.GetProjectionMatrix();
             cameraProjection[1][1] *= -1;
             cameraView = camera.GetViewMatrix();

             TransformComponent& transformComponent = selectedEntity.GetComponent<TransformComponent>();
             glm::mat4 transform = transformComponent.GetTransform();

             // Snapping
             const bool snap = Input::IsKeyPressed(Key::LeftControl);
             float snapValue = 0.5f; // Snap to 0.5m for translation/scale
             // Snap to 45 degrees for rotation
             if(mGizmoType == ImGuizmo::OPERATION::ROTATE)
                 snapValue = 45.0f;

             float snapValues[3] = { snapValue, snapValue, snapValue };
             ImGuizmo::SetGizmoSizeClipSpace(0.15f);
             ImGuizmo::Manipulate(glm::value_ptr(cameraView), glm::value_ptr(cameraProjection), static_cast<ImGuizmo::OPERATION>(mGizmoType), ImGuizmo::LOCAL, glm::value_ptr(transform), nullptr, snap ? snapValues : nullptr);
             //ImGuizmo::ViewManipulate(??, camera.GetDistance(), ImVec2(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y + 20), ImVec2(64, 64), 0x10101010);

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

         ImGui::End();
         ImGui::PopStyleVar();
    }
    void ViewportPanel::Shutdown()
    {
    }
} // namespace Surge