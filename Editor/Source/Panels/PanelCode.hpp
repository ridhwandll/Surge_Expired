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
        MaterialEditor,
        Export,
        UIEditor,
        Console,
        History
    };

    constexpr const char* PanelCodeToString(PanelCode code)
    {
        switch (code)
        {
            case PanelCode::Viewport: return "Viewport";
            case PanelCode::SceneHierarchy: return "Hierarchy";
            case PanelCode::Inspector: return "Inspector";
            case PanelCode::ContentBrowser: return "ContentBrowser & AssetRegistry";
            case PanelCode::MaterialEditor: return "MaterialEditor";
            case PanelCode::Export: return "Export";
            case PanelCode::UIEditor: return "UI Editor";
            case PanelCode::Console: return "Console";
            case PanelCode::History: return "History";
        }
        return nullptr;
    }

} // namespace Surge
