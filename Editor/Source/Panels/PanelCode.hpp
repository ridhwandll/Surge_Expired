// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once

namespace Surge
{
    enum class PanelCode
    {
        SceneHierarchy = 0,
        Inspector,
        ContentBrowser,
        MaterialEditor,
        Export,
        UIEditor,
        Console,
        History,
        AssetRegistry,
        Renderer,
        ParticleEditor,
        Physics
    };

    constexpr const char* PanelCodeToString(PanelCode code)
    {
        switch (code)
        {
            case PanelCode::SceneHierarchy: return "Hierarchy";
            case PanelCode::Inspector: return "Inspector";
            case PanelCode::ContentBrowser: return "Content Browser";
            case PanelCode::MaterialEditor: return "MaterialEditor";
            case PanelCode::Export: return "Export";
            case PanelCode::UIEditor: return "UI Editor";
            case PanelCode::Console: return "Console";
            case PanelCode::History: return "History";
            case PanelCode::AssetRegistry: return "Asset Registry";
            case PanelCode::Renderer: return "Renderer"; // Renderer settings, RenderGraph visualizer and stats
            case PanelCode::ParticleEditor: return "Particle Editor";
            case PanelCode::Physics: return "Physics"; // Project physics settings (layers, collision matrix) and stats
        }
        return nullptr;
    }

} // namespace Surge
