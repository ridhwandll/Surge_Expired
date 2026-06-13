// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Panels/IPanel.hpp"

namespace Surge
{
    class ExportPanel : public IPanel
    {
    public:
        ExportPanel() = default;
        virtual ~ExportPanel() override = default;

        virtual void Init(void* panelInitArgs) override;
        virtual void OnEvent(Event& e) override;
        virtual void Render(bool* show) override;
        virtual void Shutdown() override;

    public:
        static PanelCode GetStaticCode() { return PanelCode::Export; }
    private:
        void BuildWindows();
        void BuildAndroid();
    private:
        PanelCode mCode;
    };

} // namespace Surge