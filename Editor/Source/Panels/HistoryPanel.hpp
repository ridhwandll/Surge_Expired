// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Panels/IPanel.hpp"

namespace Surge
{
    // Lists the scene undo steps (EditorHistory), clicking a row jumps the scene to that point
    class HistoryPanel : public IPanel
    {
    public:
        HistoryPanel() = default;
        virtual ~HistoryPanel() override = default;

        virtual void Init(void* panelInitArgs) override;
        virtual void OnEvent([[maybe_unused]] Event& e) override {}
        virtual void Render(bool* show) override;
        virtual void Shutdown() override {}

        static PanelCode GetStaticCode() { return PanelCode::History; }

    private:
        PanelCode mCode;
        size_t mLastPosition = 0;
        size_t mLastSize = 0;
    };

} // namespace Surge
