// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Panels/IPanel.hpp"
#include "Panels/SceneHierarchyPanel.hpp"

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
        void DrawComponents(Entity& entity);

    private:
        PanelCode mCode;
        SceneHierarchyPanel* mHierarchy;
    };
} // namespace Surge