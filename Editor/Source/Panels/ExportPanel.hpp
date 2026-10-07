// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Panels/IPanel.hpp"
#include "Surge/Core/Scope.hpp"

namespace Surge
{
    class ExportManager;

    class ExportPanel : public IPanel
    {
    public:
        ExportPanel();
        virtual ~ExportPanel() override;

        virtual void Init(void* panelInitArgs) override;
        virtual void OnEvent(Event& e) override;
        virtual void Render(bool* show) override;
        virtual void Shutdown() override;

    public:
        static PanelCode GetStaticCode() { return PanelCode::Export; }

    private:
        PanelCode mCode;
        Scope<ExportManager> mManager;
        float mDisplayProgress = 0.0f; // Eases towards the real progress
    };

} // namespace Surge
