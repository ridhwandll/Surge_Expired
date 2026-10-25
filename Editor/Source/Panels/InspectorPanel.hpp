// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Panels/IPanel.hpp"
#include "Panels/SceneHierarchyPanel.hpp"
#include "Panels/RendererSettingsInspector.hpp"

namespace Surge
{
    class InspectorPanel : public IPanel
    {
    public:
        InspectorPanel() = default;
        ~InspectorPanel() = default;

        virtual void Init([[maybe_unused]] void* panelInitArgs) override;
        virtual void OnEvent([[maybe_unused]] Event& e) override {};
        virtual void Render(bool* show) override;
        virtual void Shutdown()  override {};

        static PanelCode GetStaticCode() { return PanelCode::Inspector; }
        void SetHierarchy(SceneHierarchyPanel* hierarchy) { mHierarchy = hierarchy; }

    private:
        void DrawEntityHeader(Entity& entity);
        void DrawComponents(Entity& entity);
        void DrawAddComponentButton(Entity& entity);

    private:
        PanelCode mCode;
        SceneHierarchyPanel* mHierarchy;

        bool mUniformScale = false;  // Transform: scale axes change together
        String mAddComponentSearch;

        // (Rid) Inspector file getting big so I started splitting it up
        RendererSettingsInspector mRendererSettings;
    };
} // namespace Surge
