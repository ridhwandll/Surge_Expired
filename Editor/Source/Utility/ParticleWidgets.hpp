// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/Particles/ParticleCurve.hpp"
#include <imgui.h>

namespace Surge::ParticleWidgets
{
    // Curve over [0, 1]: drag keys, double-click to add one, right-click a key to remove it. The selected key gets exact fields below.
    // Returns true when the curve changed (the caller re-bakes: ParticleEffect::MarkChanged)
    bool CurveEditor(const char* id, ParticleCurve& curve, float height = 64.0f);

    // Color keys under the bar, alpha keys above it: drag to move, double-click to add, right-click to remove
    bool GradientEditor(const char* id, ParticleGradient& gradient);

    // Gradient preview (checkerboard behind the alpha)
    void DrawGradientBar(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, const ParticleGradient& gradient);

    // Two column table rows (label | value), like EditorUI::TProperty. The small button on the right switches the mode
    bool FloatRow(const char* label, ParticleFloat& value, float speed = 0.05f, float min = 0.0f, float max = 0.0f, const char* format = "%.2f");
    bool ColorRow(const char* label, ParticleColor& color);
    bool CurveRow(const char* label, ParticleCurve& curve);
    bool GradientRow(const char* label, ParticleGradient& gradient);

} // namespace Surge::ParticleWidgets
