// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Core/Core.hpp"
#include "Panels/RenderProcedurePanel.hpp"
#include "Utility/ImGuiAux.hpp"

namespace Surge
{
    void RenderProcedurePanel::Init(void* panelInitArgs)
    {
        mCode = GetStaticCode();
    }

    void RenderProcedurePanel::Render(bool* show)
    {
        if (!*show)
            return;

        if (ImGui::Begin(PanelCodeToString(mCode), show))
        {
            ImGui::TextDisabled("RenderGraph passes (stub - coming soon)");
        }
        ImGui::End();
    }

    void RenderProcedurePanel::Shutdown()
    {
    }

} // namespace Surge
