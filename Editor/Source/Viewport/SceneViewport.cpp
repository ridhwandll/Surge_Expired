// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Viewport/SceneViewport.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Core/Input/Input.hpp"
#include "Surge/ECS/Components/Components.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "SurgeMath/Math.hpp"
#include "Editor.hpp"
#include "Utility/EditorUI.hpp"
#include "Gizmos/GizmoIcons.hpp"

#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>

namespace Surge
{
    SceneViewport::SceneViewport(Uint index, const EditorCamera& camera)
        : EditorViewport(ViewportType::SCENE, index), mCamera(camera)
    {
        mCamera.SetActive(false);
    }

    void SceneViewport::SetupManipulatorStyle()
    {
        ImGuizmo::Style& style = ImGuizmo::GetStyle();

        style.CenterCircleSize = 5.0f;
        style.TranslationLineThickness = 3.5f;
        style.TranslationLineArrowSize = 8.0f;
        style.RotationLineThickness = 3.0f;
        style.RotationOuterLineThickness = 3.0f;
        style.ScaleLineThickness = 3.5f;
        style.ScaleLineCircleSize = 8.0f;
        style.HatchedAxisLineThickness = 0.0f;
        style.OutlineThickness = 1.25f;
        style.InfoRounding = 4.0f;

        style.Colors[ImGuizmo::COLOR::DIRECTION_X] = EditorUI::WithAlpha(EditorUI::Colors::AxisX, 0.95f);
        style.Colors[ImGuizmo::COLOR::PLANE_X] = EditorUI::WithAlpha(EditorUI::Colors::AxisX, 0.25f);

        style.Colors[ImGuizmo::COLOR::DIRECTION_Y] = EditorUI::WithAlpha(EditorUI::Colors::AxisY, 0.95f);
        style.Colors[ImGuizmo::COLOR::PLANE_Y] = EditorUI::WithAlpha(EditorUI::Colors::AxisY, 0.25f);

        style.Colors[ImGuizmo::COLOR::DIRECTION_Z] = EditorUI::WithAlpha(EditorUI::Colors::AxisZ, 0.95f);
        style.Colors[ImGuizmo::COLOR::PLANE_Z] = EditorUI::WithAlpha(EditorUI::Colors::AxisZ, 0.25f);

        style.Colors[ImGuizmo::COLOR::SELECTION] = EditorUI::Colors::Accent;
        style.Colors[ImGuizmo::COLOR::TRANSLATION_LINE] = EditorUI::Colors::Accent;
        style.Colors[ImGuizmo::COLOR::ROTATION_USING_BORDER] = EditorUI::Colors::Accent;
        style.Colors[ImGuizmo::COLOR::ROTATION_USING_FILL] = EditorUI::WithAlpha(EditorUI::Colors::Accent, 0.25f);
        style.Colors[ImGuizmo::COLOR::SCALE_LINE] = EditorUI::WithAlpha(EditorUI::Colors::Viewport::TextFaint, 0.7f);
        style.Colors[ImGuizmo::COLOR::INACTIVE] = EditorUI::WithAlpha(EditorUI::Colors::Viewport::Inactive, 0.6f);
        style.Colors[ImGuizmo::COLOR::OUTLINE] = EditorUI::WithAlpha(EditorUI::Colors::Black, 0.55f);
        style.Colors[ImGuizmo::COLOR::INFO_BACKGROUND] = EditorUI::WithAlpha(EditorUI::Colors::Viewport::Shade, 0.85f);
        style.Colors[ImGuizmo::COLOR::TEXT] = EditorUI::Colors::Viewport::Text;
    }

    void SceneViewport::ApplySettings(const ViewportSettings& settings)
    {
        if(settings.HasCameraView)
            mCamera.SetView(settings.CameraFocalPoint, settings.CameraDistance, settings.CameraPitch, settings.CameraYaw);
        mCamera.SetCameraSpeed(settings.CameraSpeed);
    }

    void SceneViewport::CaptureSettings(ViewportSettings& settings) const
    {
        settings.HasCameraView = true;
        settings.CameraFocalPoint = mCamera.GetFocalPoint();
        settings.CameraDistance = mCamera.GetDistance();
        settings.CameraPitch = mCamera.GetPitch();
        settings.CameraYaw = mCamera.GetYaw();
        settings.CameraSpeed = mCamera.GetCameraSpeed();
    }

    void SceneViewport::UpdateCamera(bool ownsInput)
    {
        mCamera.SetActive(ownsInput);
        mCamera.OnUpdate(); // Inactive cameras still settle their damping
    }

    void SceneViewport::UpdateViewCamera(Scene&)
    {
        Core::GetRenderer()->SetViewCamera(mView, mCamera.GetViewMatrix(), mCamera.GetProjectionMatrix(), mCamera.GetNearAndFarPlane());
    }

    void SceneViewport::OnEvent(Event& e)
    {
        EditorViewport::OnEvent(e);
        if(!IsHovered())
            return;

        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& keyEvent) {
            // F (focus) lives in Editor::HandleShortcuts, it also works from the Hierarchy
            if(mGizmoInUse || Input::IsMouseButtonPressed(Mouse::ButtonRight) || Input::IsKeyPressed(Key::LeftControl))
                return;

            EditorSettings& settings = static_cast<Editor*>(Core::GetClient())->GetSettings();
            switch(keyEvent.GetKeyCode())
            {
                case Key::Q: settings.GizmoOperation = -1; break;
                case Key::W: settings.GizmoOperation = ImGuizmo::OPERATION::TRANSLATE; break;
                case Key::E: settings.GizmoOperation = ImGuizmo::OPERATION::ROTATE; break;
                case Key::R: settings.GizmoOperation = ImGuizmo::OPERATION::SCALE; break;
                case Key::T: settings.GizmoOperation = ImGuizmo::OPERATION::UNIVERSAL; break;
                case Key::X: settings.GizmoWorldSpace = !settings.GizmoWorldSpace; break;
                default: break;
            }
        });
    }

    void SceneViewport::OnViewportGUI(const ImVec2& imageMin, const ImVec2& imageSize)
    {
        if(mCamera.GetViewportSize() != glm::vec2(imageSize.x, imageSize.y))
            mCamera.SetViewportSize({ imageSize.x, imageSize.y });

        Editor* editor = static_cast<Editor*>(Core::GetClient());
        EditorSettings& settings = editor->GetSettings();
        Scene& scene = *editor->GetContextScene();
        Entity selectedEntity = editor->GetSelectedEntity();
        const bool hovered = IsHovered();

        // Editor gizmos: component icons, camera frustums, light/audio ranges
        SceneGizmos& gizmos = editor->GetGizmos();
        const bool allowIconHover = hovered && !ImGuizmo::IsUsingAny() && !ImGui::IsAnyItemHovered() && !Input::IsMouseButtonPressed(Mouse::ButtonRight);
        gizmos.DrawOverlay(scene, selectedEntity, mCamera.GetViewMatrix(), mCamera.GetProjectionMatrix(), mCamera.GetNearAndFarPlane().x, imageMin, imageSize, allowIconHover);

        // Toolbar
        constexpr float padding = 3.0f;
        ImGui::SetCursorScreenPos(ImVec2(imageMin.x + padding, imageMin.y + padding));
        DrawToolbar(settings);

        // Scene name (* = unsaved changes), the running world while playing
        const ViewportManager& viewports = editor->GetViewports();
        const String label = editor->IsPlaying() ? String(editor->IsPaused() ? "RUNTIME (PAUSED)" : "RUNTIME")
                                                 : (editor->GetHistory().IsDirty() ? viewports.GetSceneName() + " *" : viewports.GetSceneName());
        DrawCornerLabel(label.c_str(), imageMin, imageSize);

        // Transform manipulator, one ImGuizmo ID per view so a drag in one view doesn't move the others
        ImGuizmo::PushID(static_cast<int>(GetIndex()));
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(imageMin.x, imageMin.y, imageSize.x, imageSize.y);

        const bool manipulatorActive = selectedEntity && settings.GizmoOperation > 0;
        if(manipulatorActive)
        {
            glm::mat4 cameraView = mCamera.GetViewMatrix();
            glm::mat4 cameraProjection = mCamera.GetProjectionMatrix();
            cameraProjection[1][1] *= -1;

            TransformComponent& transformComponent = selectedEntity.GetComponent<TransformComponent>();
            glm::mat4 transform = transformComponent.GetTransform();

            // Snapping, holding Ctrl inverts the toolbar toggle
            const bool snap = settings.SnapEnabled != Input::IsKeyPressed(Key::LeftControl);
            float snapValue = settings.SnapTranslate;
            if(settings.GizmoOperation == ImGuizmo::OPERATION::ROTATE)
                snapValue = settings.SnapRotate;
            else if(settings.GizmoOperation == ImGuizmo::OPERATION::SCALE)
                snapValue = settings.SnapScale;

            float snapValues[3] = { snapValue, snapValue, snapValue };
            const ImGuizmo::MODE space = settings.GizmoWorldSpace ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
            ImGuizmo::SetGizmoSizeClipSpace(0.15f);
            ImGuizmo::Manipulate(glm::value_ptr(cameraView), glm::value_ptr(cameraProjection), static_cast<ImGuizmo::OPERATION>(settings.GizmoOperation), space, glm::value_ptr(transform), nullptr, snap ? snapValues : nullptr);

            mGizmoInUse = ImGuizmo::IsUsing();
            if(mGizmoInUse)
            {
                RelationshipComponent& relationship = selectedEntity.GetComponent<RelationshipComponent>();
                if(relationship.Parent != entt::null)
                {
                    Entity parentEntity(static_cast<entt::entity>(relationship.Parent), &scene);
                    transform = glm::inverse(parentEntity.GetComponent<TransformComponent>().GetTransform()) * transform;
                }

                glm::vec3 translation, rotation, scale;
                Math::DecomposeTransform(transform, translation, rotation, scale);

                const glm::vec3 deltaRotation = glm::degrees(rotation) - transformComponent.Rotation;
                transformComponent.Position = translation;
                transformComponent.Rotation += deltaRotation;
                transformComponent.Scale = scale;
                transformComponent.MarkDirty();
            }
        }
        else
            mGizmoInUse = false;

        HandleSelectionClick(selectedEntity, manipulatorActive, imageMin, imageSize);
        ImGuizmo::PopID();
    }

    void SceneViewport::HandleSelectionClick(Entity& selectedEntity, bool manipulatorActive, const ImVec2& imageMin, const ImVec2& imageSize)
    {
        const ImVec2 mouse = ImGui::GetMousePos();
        if(ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            // The manipulator, overlay widgets and Alt (camera orbit) own the press
            const bool insideImage = mouse.x >= imageMin.x && mouse.y >= imageMin.y && mouse.x < imageMin.x + imageSize.x && mouse.y < imageMin.y + imageSize.y;
            const bool manipulatorHovered = manipulatorActive && (ImGuizmo::IsOver() || ImGuizmo::IsUsing());
            mPickPending = IsHovered() && insideImage && !manipulatorHovered && !ImGui::IsAnyItemHovered() && !Input::IsKeyPressed(Key::LeftAlt);
            mPickPressPos = mouse;
        }

        if(!mPickPending || !ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            return;

        mPickPending = false;
        const float dragX = mouse.x - mPickPressPos.x;
        const float dragY = mouse.y - mPickPressPos.y;
        if(mGizmoInUse || dragX * dragX + dragY * dragY > 16.0f) // Drag threshold (4px) to avoid accidental clicks while moving the camera
            return;

        Editor* editor = static_cast<Editor*>(Core::GetClient());
        Entity picked = editor->GetGizmos().GetHoveredEntity(); // Icons are drawn on top of everything, so they win over the geometry behind them
        if(!picked)
        {
            const PickRay ray = ScenePicker::ScreenToRay({ mouse.x, mouse.y }, { imageMin.x, imageMin.y }, { imageSize.x, imageSize.y }, mCamera.GetViewMatrix(), mCamera.GetProjectionMatrix());
            picked = mPicker.Pick(*editor->GetContextScene(), ray, mCamera.GetViewMatrix(), { mouse.x, mouse.y }, selectedEntity);
        }

        if(picked != selectedEntity)
            editor->SelectEntity(picked); // Empty space clears the selection
    }

    void SceneViewport::DrawToolbar(EditorSettings& settings)
    {
        struct Tool
        {
            ToolIcon Icon;
            int Operation;
            const char* Tooltip;
        };
        static constexpr Tool sTools[] = {
            { ToolIcon::SELECT, -1, "Select only (Q)" },
            { ToolIcon::MOVE, ImGuizmo::OPERATION::TRANSLATE, "Move (W)" },
            { ToolIcon::ROTATE, ImGuizmo::OPERATION::ROTATE, "Rotate (E)" },
            { ToolIcon::SCALE, ImGuizmo::OPERATION::SCALE, "Scale (R)" },
            { ToolIcon::UNIVERSAL, ImGuizmo::OPERATION::UNIVERSAL, "Move + Rotate + Scale (T)" },
        };
        Editor* editor = static_cast<Editor*>(Core::GetClient());
        Scope<GraphicsRHI>& rhi = Core::GetRenderer()->GetRHI();

        // Highlighted like the selection in the Hierarchy
        auto toggleButton = [](const char* label, bool active, const char* tooltip) -> bool {
            if(active)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, EditorUI::Colors::Selection);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorUI::Colors::Selection);
                ImGui::PushStyleColor(ImGuiCol_Text, EditorUI::Colors::Ink);
            }
            const bool pressed = ImGui::Button(label);
            if(active)
                ImGui::PopStyleColor(3);
            EditorUI::ItemTooltip(tooltip);
            return pressed;
        };

        ImGui::PushStyleColor(ImGuiCol_Button, EditorUI::Colors::Overlay);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, EditorUI::Colors::Overlay);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f, 0.0f));

        // Tool icons, square-ish frame height buttons
        const float frameHeight = ImGui::GetFrameHeight();
        const ImVec2 toolButtonSize(frameHeight * 1.15f, frameHeight);
        for(const Tool& tool : sTools)
        {
            const bool active = settings.GizmoOperation == tool.Operation;
            if(active)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, EditorUI::Colors::Selection);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorUI::Colors::Selection);
            }

            ImGui::PushID(tool.Operation);
            const ImVec4 tint = active ? EditorUI::Colors::Ink : EditorUI::Colors::Tone85;
            if(GizmoIcons::IconButton("##Tool", rhi->GetImGuiImage(editor->GetToolIcon(tool.Icon)), toolButtonSize, tint, 0.9f))
                settings.GizmoOperation = tool.Operation;
            ImGui::PopID();

            if(active)
                ImGui::PopStyleColor(2);
            EditorUI::ItemTooltip(tool.Tooltip);
            ImGui::SameLine();
        }

        ImGui::SameLine(0.0f, 8.0f);
        if(toggleButton(settings.GizmoWorldSpace ? "World" : "Local", false, "Gizmo space: local / world (X)"))
            settings.GizmoWorldSpace = !settings.GizmoWorldSpace;

        ImGui::SameLine(0.0f, 8.0f);
        if(toggleButton("Snap", settings.SnapEnabled, "Snapping, holding Ctrl inverts it"))
            settings.SnapEnabled = !settings.SnapEnabled;
        ImGui::SameLine();
        if(ImGui::ArrowButton("##SnapSettings", ImGuiDir_Down))
            ImGui::OpenPopup("SnapSettingsPopup");

        ImGui::SameLine(0.0f, 8.0f);
        if(toggleButton("Camera", false, "Scene camera settings"))
            ImGui::OpenPopup("CameraSettingsPopup");

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);

        EditorUI::StyledPopupVars::Push();
        if(ImGui::BeginPopup("SnapSettingsPopup"))
        {
            ImGui::SetNextItemWidth(120.0f);
            ImGui::DragFloat("Move", &settings.SnapTranslate, 0.05f, 0.001f, 1000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SetNextItemWidth(120.0f);
            ImGui::DragFloat("Rotate (deg)", &settings.SnapRotate, 1.0f, 0.01f, 360.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SetNextItemWidth(120.0f);
            ImGui::DragFloat("Scale", &settings.SnapScale, 0.01f, 0.001f, 100.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
            ImGui::EndPopup();
        }
        if(ImGui::BeginPopup("CameraSettingsPopup"))
        {
            float speed = mCamera.GetCameraSpeed();
            ImGui::SetNextItemWidth(160.0f);
            if(ImGui::SliderFloat("Fly Speed", &speed, 0.0005f, 2.0f, "%.4f", ImGuiSliderFlags_Logarithmic))
                mCamera.SetCameraSpeed(speed);
            ImGui::TextDisabled("RMB + scroll changes it while flying");

            if(ImGui::Button("Reset View"))
                mCamera.SetView(glm::vec3(0.0f), glm::length(glm::vec3(-5.0f, 5.0f, 5.0f)), glm::quarter_pi<float>(), 3.0f * glm::quarter_pi<float>()); // EditorCamera's initial view
            ImGui::SameLine();
            if(ImGui::Button("Top"))
                mCamera.SetView(mCamera.GetFocalPoint(), mCamera.GetDistance(), glm::half_pi<float>() - 0.01f, 0.0f);
            ImGui::SameLine();
            if(ImGui::Button("Front"))
                mCamera.SetView(mCamera.GetFocalPoint(), mCamera.GetDistance(), 0.0f, 0.0f);
            ImGui::SameLine();
            if(ImGui::Button("Side"))
                mCamera.SetView(mCamera.GetFocalPoint(), mCamera.GetDistance(), 0.0f, glm::half_pi<float>());
            ImGui::EndPopup();
        }
        EditorUI::StyledPopupVars::Pop();
    }

} // namespace Surge
