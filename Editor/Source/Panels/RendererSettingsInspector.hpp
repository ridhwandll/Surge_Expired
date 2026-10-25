// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/ECS/Scene.hpp"

namespace Surge
{
    struct RendererSettingsComponent;
    struct RenderSettings;

    // Inspector rows of RendererSettingsComponent: a Desktop | Mobile profile switch (also what every viewport previews), then every
    // setting with a small hint under it. Drawn inside the Inspector's two column component table
    class RendererSettingsInspector
    {
    public:
        void Draw(Entity entity, RendererSettingsComponent& component);

    private:
        void DrawProfileSwitch(RendererSettingsComponent& component);
        void DrawGeneral(RenderSettings& settings);
        void DrawShadows(RenderSettings& settings);
        void DrawAmbientOcclusion(RenderSettings& settings);
        void DrawBloom(RenderSettings& settings);
        void DrawPostProcess(RenderSettings& settings);

    private:
        bool mShowAdvanced = false;    // Knobs the quality presets set, saved like the rest

        // Applied on drag release: every change rebuilds the scene targets of all graphs
        float mRenderScale = 1.0f;
        bool mEditingRenderScale = false;
    };

} // namespace Surge
