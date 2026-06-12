// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Events/Event.hpp"
#include "Panels/PanelCode.hpp" // For convenience

namespace Surge
{
    class IPanel
    {
    public:
        IPanel() = default;
        virtual ~IPanel() = default;

        virtual void Init(void* panelInitArgs) = 0;
        virtual void OnEvent(Event& e) = 0;
        virtual void Render(bool* show) = 0;
        virtual void Shutdown() = 0;
    };
} // namespace Surge