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
            //if (!Input::IsMouseButtonPressed(Mouse::ButtonRight))
            //{
            //    switch (keyEvent.GetKeyCode())
            //    {
            //        // Gizmos
            //        case Key::Q:
            //        {
            //            if (!mGizmoInUse)
            //                mGizmoType = -20;
            //            break;
            //        }
            //        case Key::W:
            //        {
            //            if (!mGizmoInUse)
            //                mGizmoType = ImGuizmo::OPERATION::TRANSLATE;
            //            break;
            //        }
            //        case Key::E:
            //        {
            //            if (!mGizmoInUse)
            //                mGizmoType = ImGuizmo::OPERATION::ROTATE;
            //            break;
            //        }
            //        case Key::R:
            //        {
            //            if (!mGizmoInUse)
            //                mGizmoType = ImGuizmo::OPERATION::SCALE;
            //            break;
            //        }
            //    }
            //}
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
 
         ImGui::End();
         ImGui::PopStyleVar();
    }
    void ViewportPanel::Shutdown()
    {
    }
} // namespace Surge