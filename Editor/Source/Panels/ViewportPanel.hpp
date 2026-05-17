// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Panels/IPanel.hpp"
#include "Panels/SceneHierarchyPanel.hpp"
#include <glm/glm.hpp>

namespace Surge
{
    class ViewportPanel : public IPanel
    {
    public:
        ViewportPanel() = default;
        virtual ~ViewportPanel() override = default;

        virtual void Init(void* panelInitArgs) override;
        virtual void OnEvent(Event& e) override;
        virtual void Render(bool* show) override;
        virtual void Shutdown() override;

        const glm::vec2& GetViewportSize() const { return mViewportSize; }
        bool IsViewportHovered() const { return mIsViewportHovered; }
    public:
        static PanelCode GetStaticCode() { return PanelCode::Viewport; }

    private:
        PanelCode mCode;
        glm::vec2 mViewportSize = glm::vec2(0.0f);
        int mGizmoType = -1;
        bool mGizmoInUse = false;
        bool mIsViewportHovered = false;
        SceneHierarchyPanel* mSceneHierarchy;
        EditorCamera* mEditorCam;
    };
} // namespace Surge