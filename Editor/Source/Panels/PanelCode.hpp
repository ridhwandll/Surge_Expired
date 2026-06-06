// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once

namespace Surge
{
    enum class PanelCode
    {
        Viewport = 0,
        SceneHierarchy,
        Inspector,
        ContentBrowser,
        MaterialEditor
    };

    constexpr const char* PanelCodeToString(PanelCode code)
    {
        switch (code)
        {
            case PanelCode::Viewport: return "Viewport";
            case PanelCode::SceneHierarchy: return "Hierarchy";
            case PanelCode::Inspector: return "Inspector";
            case PanelCode::ContentBrowser: return "ContentBrowser";
            case PanelCode::MaterialEditor: return "MaterialEditor";
        }
        return nullptr;
    }

} // namespace Surge
