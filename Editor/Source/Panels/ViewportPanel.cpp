// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Panels/ViewportPanel.hpp"
#include "Surge/Graphics/Interface/Image.hpp"
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
    }

    void ViewportPanel::OnEvent(Event& e)
    {
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<KeyPressedEvent>([&](KeyPressedEvent& keyEvent) -> bool {
            if (!Input::IsMouseButtonPressed(Mouse::ButtonRight))
            {
                switch (keyEvent.GetKeyCode())
                {
                    // Gizmos
                    case Key::Q:
                    {
                        if (!mGizmoInUse)
                            mGizmoType = -20;
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
            //const Ref<Image2D>& outputImage = Core::GetRenderer()->GetFinalPassFramebuffer()->GetColorAttachment(0);
            Ref<Image2D> outputImage = nullptr;
            mViewportSize = {ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y};
            ImGuiAux::Image(outputImage, mViewportSize);

            // Entity transform
            Entity& selectedEntity = mSceneHierarchy->GetSelectedEntity();
            if (selectedEntity)
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

                Scene* activeScene = mSceneHierarchy->GetSceneContext();
                TransformComponent& transformComponent = selectedEntity.GetComponent<TransformComponent>();
                glm::mat4 transform = transformComponent.GetTransform();

                // Snapping
                const bool snap = Input::IsKeyPressed(Key::LeftControl);

                float snapValue = 0.5f; // Snap to 0.5m for translation/scale

                // Snap to 45 degrees for rotation
                if (mGizmoType == ImGuizmo::OPERATION::ROTATE)
                    snapValue = 45.0f;

                float snapValues[3] = {snapValue, snapValue, snapValue};

                ImGuizmo::Manipulate(glm::value_ptr(cameraView), glm::value_ptr(cameraProjection), static_cast<ImGuizmo::OPERATION>(mGizmoType), ImGuizmo::LOCAL, glm::value_ptr(transform), nullptr, snap ? snapValues : nullptr);
                //ImGuizmo::ViewManipulate(??, camera.GetDistance(), ImVec2(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y + 20), ImVec2(64, 64), 0x10101010);

                if (ImGuizmo::IsUsing())
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

        ImGui::End();
        ImGui::PopStyleVar();
    }
    void ViewportPanel::Shutdown()
    {
    }
} // namespace Surge