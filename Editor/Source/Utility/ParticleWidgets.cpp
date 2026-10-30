// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Utility/ParticleWidgets.hpp"
#include "Utility/EditorUI.hpp"
#include <imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>

namespace Surge::ParticleWidgets
{
    namespace
    {
        namespace Colors = EditorUI::Colors;
        using EditorUI::ToU32;
        using EditorUI::WithAlpha;
        constexpr ImU32 CURVE_COL = ToU32(Colors::Curve);
        constexpr ImU32 SELECTED = ToU32(Colors::Accent);
        constexpr float KEY_RADIUS = 4.0f;
        constexpr float PICK_RADIUS = 7.0f;

        float Distance(const ImVec2& a, const ImVec2& b)
        {
            const float dx = a.x - b.x;
            const float dy = a.y - b.y;
            return std::sqrt(dx * dx + dy * dy);
        }
    } // namespace

    //////////////////////////////////////////////////////////////////////////
    // Curve
    //////////////////////////////////////////////////////////////////////////

    bool CurveEditor(const char* id, ParticleCurve& curve, float height)
    {
        bool changed = false;
        ImGui::PushID(id);

        ImGuiStorage* storage = ImGui::GetStateStorage();
        const ImGuiID dragId = ImGui::GetID("##Drag");
        const ImGuiID selectedId = ImGui::GetID("##Selected");
        const ImGuiID rangeLowId = ImGui::GetID("##Low");
        const ImGuiID rangeHighId = ImGui::GetID("##High");

        int dragging = storage->GetInt(dragId, -1);
        int selected = storage->GetInt(selectedId, -1);
        if(selected >= static_cast<int>(curve.KeyCount))
            selected = -1;

        // Vertical range covers [0, 1] and every key, frozen while a key is dragged so it does not run away under the mouse
        float low = std::min(0.0f, curve.GetMinValue());
        float high = std::max(1.0f, curve.GetMaxValue());
        if(dragging >= 0)
        {
            low = storage->GetFloat(rangeLowId, low);
            high = storage->GetFloat(rangeHighId, high);
        }
        if(high - low < 1e-3f)
            high = low + 1.0f;

        const ImVec2 size(std::max(ImGui::GetContentRegionAvail().x, 40.0f), height);
        ImGui::InvisibleButton("##Curve", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        const bool hovered = ImGui::IsItemHovered();
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        constexpr float pad = 6.0f;

        const auto toScreen = [&](float t, float v) {
            return ImVec2(min.x + pad + t * (max.x - min.x - pad * 2.0f), max.y - pad - (v - low) / (high - low) * (max.y - min.y - pad * 2.0f));
        };
        const auto fromScreen = [&](const ImVec2& p) {
            const float t = (p.x - min.x - pad) / (max.x - min.x - pad * 2.0f);
            const float v = low + (max.y - pad - p.y) / (max.y - min.y - pad * 2.0f) * (high - low);
            return ImVec2(glm::clamp(t, 0.0f, 1.0f), glm::clamp(v, low, high));
        };

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(min, max, ToU32(Colors::Tone09), 3.0f);
        for(int i = 1; i < 4; i++)
        {
            const float x = min.x + pad + (max.x - min.x - pad * 2.0f) * static_cast<float>(i) * 0.25f;
            drawList->AddLine(ImVec2(x, min.y + 1.0f), ImVec2(x, max.y - 1.0f), ToU32(WithAlpha(Colors::Ink, 0.07f)));
        }
        if(low < 0.0f && high > 0.0f)
        {
            const float zeroY = toScreen(0.0f, 0.0f).y;
            drawList->AddLine(ImVec2(min.x + 1.0f, zeroY), ImVec2(max.x - 1.0f, zeroY), ToU32(WithAlpha(Colors::Ink, 0.16f)));
        }

        // Curve, sampled from the keys (exact, not the baked table)
        constexpr int samples = 48;
        ImVec2 points[samples];
        for(int i = 0; i < samples; i++)
        {
            const float t = static_cast<float>(i) / static_cast<float>(samples - 1);
            points[i] = toScreen(t, curve.EvaluateKeys(t));
        }
        drawList->AddPolyline(points, samples, CURVE_COL, ImDrawFlags_None, 2.0f);

        // Key under the mouse
        const ImVec2 mouse = ImGui::GetMousePos();
        int hoveredKey = -1;
        float bestDistance = PICK_RADIUS;
        for(Uint i = 0; i < curve.KeyCount; i++)
        {
            const float d = Distance(mouse, toScreen(curve.Keys[i].Time, curve.Keys[i].Value));
            if(d < bestDistance)
            {
                bestDistance = d;
                hoveredKey = static_cast<int>(i);
            }
        }

        if(hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && hoveredKey >= 0)
        {
            dragging = hoveredKey;
            selected = hoveredKey;
            storage->SetFloat(rangeLowId, low);
            storage->SetFloat(rangeHighId, high);
        }
        else if(hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && hoveredKey < 0)
        {
            const ImVec2 tv = fromScreen(mouse);
            if(curve.AddKey(tv.x, curve.EvaluateKeys(tv.x)))
            {
                changed = true;
                for(Uint i = 0; i < curve.KeyCount; i++)
                {
                    if(curve.Keys[i].Time == tv.x)
                        selected = static_cast<int>(i);
                }
            }
        }
        else if(hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && hoveredKey >= 0 && curve.KeyCount > 1)
        {
            curve.RemoveKey(static_cast<Uint>(hoveredKey));
            selected = -1;
            changed = true;
        }

        if(dragging >= 0 && dragging < static_cast<int>(curve.KeyCount))
        {
            if(ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                // A key stays between its neighbours, the order never changes while dragging
                const ImVec2 tv = fromScreen(mouse);
                const float lowTime = dragging > 0 ? curve.Keys[dragging - 1].Time : 0.0f;
                const float highTime = dragging + 1 < static_cast<int>(curve.KeyCount) ? curve.Keys[dragging + 1].Time : 1.0f;
                ParticleCurve::Key& key = curve.Keys[dragging];
                const float newTime = glm::clamp(tv.x, lowTime, highTime);
                if(newTime != key.Time || tv.y != key.Value)
                {
                    key.Time = newTime;
                    key.Value = tv.y;
                    changed = true;
                }
            }
            else
                dragging = -1;
        }
        else
            dragging = -1;

        for(Uint i = 0; i < curve.KeyCount; i++)
        {
            const ImVec2 p = toScreen(curve.Keys[i].Time, curve.Keys[i].Value);
            const bool isSelected = static_cast<int>(i) == selected;
            drawList->AddCircleFilled(p, KEY_RADIUS + (static_cast<int>(i) == hoveredKey ? 1.5f : 0.0f), isSelected ? SELECTED : ToU32(Colors::Tone90), 12);
        }
        drawList->AddRect(min, max, hovered ? ToU32(Colors::Tone45) : ToU32(Colors::Tone28), 3.0f);

        // Range labels
        {
            char text[32];
            snprintf(text, sizeof(text), "%.2f", high);
            drawList->AddText(ImVec2(min.x + 4.0f, min.y + 2.0f), ToU32(WithAlpha(Colors::Tone60, 0.78f)), text);
            snprintf(text, sizeof(text), "%.2f", low);
            drawList->AddText(ImVec2(min.x + 4.0f, max.y - ImGui::GetFontSize() - 2.0f), ToU32(WithAlpha(Colors::Tone60, 0.78f)), text);
        }

        if(hovered && hoveredKey >= 0 && dragging < 0)
            ImGui::SetTooltip("t %.2f  |  %.3f\nRight-click to remove", curve.Keys[hoveredKey].Time, curve.Keys[hoveredKey].Value);
        else if(hovered && dragging < 0)
            EditorUI::DelayedToolTip("Double-click to add a key");

        // Exact values of the selected key
        if(selected >= 0)
        {
            ParticleCurve::Key& key = curve.Keys[selected];
            const float lowTime = selected > 0 ? curve.Keys[selected - 1].Time : 0.0f;
            const float highTime = selected + 1 < static_cast<int>(curve.KeyCount) ? curve.Keys[selected + 1].Time : 1.0f;
            const float half = (ImGui::GetContentRegionAvail().x - 4.0f) * 0.5f;
            ImGui::SetNextItemWidth(half);
            changed |= ImGui::DragFloat("##KeyTime", &key.Time, 0.005f, lowTime, highTime, "t %.3f", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SameLine(0.0f, 4.0f);
            ImGui::SetNextItemWidth(half);
            changed |= ImGui::DragFloat("##KeyValue", &key.Value, 0.01f, 0.0f, 0.0f, "%.3f");
        }

        storage->SetInt(dragId, dragging);
        storage->SetInt(selectedId, selected);
        ImGui::PopID();
        return changed;
    }

    //////////////////////////////////////////////////////////////////////////
    // Gradient
    //////////////////////////////////////////////////////////////////////////

    void DrawGradientBar(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, const ParticleGradient& gradient)
    {
        // Checkerboard, shows through where alpha is low
        const float cell = std::max((max.y - min.y) * 0.5f, 4.0f);
        drawList->PushClipRect(min, max, true);
        for(float y = min.y; y < max.y; y += cell)
        {
            for(float x = min.x; x < max.x; x += cell)
            {
                const bool dark = (static_cast<int>((x - min.x) / cell) + static_cast<int>((y - min.y) / cell)) % 2 == 0;
                drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + cell, y + cell), ToU32(dark ? Colors::Tone35 : Colors::Tone55));
            }
        }

        constexpr int segments = 32;
        const float width = max.x - min.x;
        for(int i = 0; i < segments; i++)
        {
            const float t0 = static_cast<float>(i) / segments;
            const float t1 = static_cast<float>(i + 1) / segments;
            const glm::vec4 c0 = glm::clamp(gradient.EvaluateKeys(t0), 0.0f, 1.0f);
            const glm::vec4 c1 = glm::clamp(gradient.EvaluateKeys(t1), 0.0f, 1.0f);
            const ImU32 left = ImGui::ColorConvertFloat4ToU32(ImVec4(c0.r, c0.g, c0.b, c0.a));
            const ImU32 right = ImGui::ColorConvertFloat4ToU32(ImVec4(c1.r, c1.g, c1.b, c1.a));
            EditorUI::AddHorizontalGradient(drawList, ImVec2(min.x + width * t0, min.y), ImVec2(min.x + width * t1, max.y), left, right);
        }
        drawList->PopClipRect();
        drawList->AddRect(min, max, ToU32(Colors::Tone28), 2.0f);
    }

    bool GradientEditor(const char* id, ParticleGradient& gradient)
    {
        bool changed = false;
        ImGui::PushID(id);

        ImGuiStorage* storage = ImGui::GetStateStorage();
        const ImGuiID kindId = ImGui::GetID("##Kind");   // 0 none, 1 color key, 2 alpha key
        const ImGuiID indexId = ImGui::GetID("##Index");
        const ImGuiID dragId = ImGui::GetID("##Drag");
        int kind = storage->GetInt(kindId, 0);
        int index = storage->GetInt(indexId, -1);
        bool dragging = storage->GetBool(dragId, false);
        if((kind == 1 && index >= static_cast<int>(gradient.ColorKeyCount)) || (kind == 2 && index >= static_cast<int>(gradient.AlphaKeyCount)) || index < 0)
            kind = 0;

        constexpr float markerArea = 9.0f;
        constexpr float barHeight = 18.0f;
        const ImVec2 size(std::max(ImGui::GetContentRegionAvail().x, 40.0f), barHeight + markerArea * 2.0f);
        ImGui::InvisibleButton("##Gradient", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        const bool hovered = ImGui::IsItemHovered();
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        constexpr float pad = 5.0f;
        const ImVec2 barMin(min.x + pad, min.y + markerArea);
        const ImVec2 barMax(max.x - pad, max.y - markerArea);
        const float barWidth = barMax.x - barMin.x;

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        DrawGradientBar(drawList, barMin, barMax, gradient);

        const ImVec2 mouse = ImGui::GetMousePos();
        const float mouseTime = glm::clamp((mouse.x - barMin.x) / barWidth, 0.0f, 1.0f);
        const bool inAlphaArea = mouse.y < barMin.y;
        const bool inColorArea = mouse.y > barMax.y;

        // Marker under the mouse: alpha keys above the bar, color keys below it
        int hoveredKind = 0;
        int hoveredIndex = -1;
        float bestDistance = PICK_RADIUS;
        for(Uint i = 0; i < gradient.AlphaKeyCount; i++)
        {
            const float d = Distance(mouse, ImVec2(barMin.x + gradient.AlphaKeys[i].Time * barWidth, barMin.y - markerArea * 0.5f));
            if(d < bestDistance) { bestDistance = d; hoveredKind = 2; hoveredIndex = static_cast<int>(i); }
        }
        for(Uint i = 0; i < gradient.ColorKeyCount; i++)
        {
            const float d = Distance(mouse, ImVec2(barMin.x + gradient.ColorKeys[i].Time * barWidth, barMax.y + markerArea * 0.5f));
            if(d < bestDistance) { bestDistance = d; hoveredKind = 1; hoveredIndex = static_cast<int>(i); }
        }

        if(hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && hoveredKind != 0)
        {
            kind = hoveredKind;
            index = hoveredIndex;
            dragging = true;
        }
        else if(hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && hoveredKind == 0)
        {
            const glm::vec4 value = gradient.EvaluateKeys(mouseTime);
            if(inAlphaArea ? gradient.AddAlphaKey(mouseTime, value.a) : gradient.AddColorKey(mouseTime, glm::vec3(value)))
            {
                changed = true;
                kind = inAlphaArea ? 2 : 1;
                const Uint count = inAlphaArea ? gradient.AlphaKeyCount : gradient.ColorKeyCount;
                for(Uint i = 0; i < count; i++)
                {
                    const float t = inAlphaArea ? gradient.AlphaKeys[i].Time : gradient.ColorKeys[i].Time;
                    if(t == mouseTime)
                        index = static_cast<int>(i);
                }
            }
        }
        else if(hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && hoveredKind != 0)
        {
            if(hoveredKind == 1 && gradient.ColorKeyCount > 1)
                gradient.RemoveColorKey(static_cast<Uint>(hoveredIndex));
            else if(hoveredKind == 2 && gradient.AlphaKeyCount > 1)
                gradient.RemoveAlphaKey(static_cast<Uint>(hoveredIndex));
            kind = 0;
            changed = true;
        }
        (void)inColorArea;

        // Dragging moves a key in time, between its neighbours (the order never changes)
        if(dragging && kind != 0 && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            float* time = kind == 1 ? &gradient.ColorKeys[index].Time : &gradient.AlphaKeys[index].Time;
            const Uint count = kind == 1 ? gradient.ColorKeyCount : gradient.AlphaKeyCount;
            const float lowTime = index > 0 ? (kind == 1 ? gradient.ColorKeys[index - 1].Time : gradient.AlphaKeys[index - 1].Time) : 0.0f;
            const float highTime = index + 1 < static_cast<int>(count) ? (kind == 1 ? gradient.ColorKeys[index + 1].Time : gradient.AlphaKeys[index + 1].Time) : 1.0f;
            const float newTime = glm::clamp(mouseTime, lowTime, highTime);
            if(newTime != *time)
            {
                *time = newTime;
                changed = true;
            }
        }
        else
            dragging = false;

        // Markers
        for(Uint i = 0; i < gradient.AlphaKeyCount; i++)
        {
            const float x = barMin.x + gradient.AlphaKeys[i].Time * barWidth;
            const float a = glm::clamp(gradient.AlphaKeys[i].Alpha, 0.0f, 1.0f);
            const ImU32 fill = ImGui::ColorConvertFloat4ToU32(ImVec4(a, a, a, 1.0f));
            const bool isSelected = kind == 2 && index == static_cast<int>(i);
            drawList->AddTriangleFilled(ImVec2(x - 5.0f, min.y), ImVec2(x + 5.0f, min.y), ImVec2(x, barMin.y), fill);
            drawList->AddTriangle(ImVec2(x - 5.0f, min.y), ImVec2(x + 5.0f, min.y), ImVec2(x, barMin.y), isSelected ? SELECTED : ToU32(Colors::Tone07), isSelected ? 2.0f : 1.0f);
        }
        for(Uint i = 0; i < gradient.ColorKeyCount; i++)
        {
            const float x = barMin.x + gradient.ColorKeys[i].Time * barWidth;
            const glm::vec3 c = glm::clamp(gradient.ColorKeys[i].Color, 0.0f, 1.0f);
            const ImU32 fill = ImGui::ColorConvertFloat4ToU32(ImVec4(c.r, c.g, c.b, 1.0f));
            const bool isSelected = kind == 1 && index == static_cast<int>(i);
            drawList->AddTriangleFilled(ImVec2(x, barMax.y), ImVec2(x + 5.0f, max.y), ImVec2(x - 5.0f, max.y), fill);
            drawList->AddTriangle(ImVec2(x, barMax.y), ImVec2(x + 5.0f, max.y), ImVec2(x - 5.0f, max.y), isSelected ? SELECTED : ToU32(Colors::Tone07), isSelected ? 2.0f : 1.0f);
        }

        if(hovered && !dragging)
            EditorUI::DelayedToolTip("Color keys below, alpha keys above\nDouble-click to add, right-click to remove");

        // Selected key
        if(kind == 1)
        {
            ParticleGradient::ColorKey& key = gradient.ColorKeys[index];
            const float half = (ImGui::GetContentRegionAvail().x - 4.0f) * 0.5f;
            ImGui::SetNextItemWidth(half);
            changed |= ImGui::ColorEdit3("##KeyColor", glm::value_ptr(key.Color), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
            ImGui::SameLine(0.0f, 4.0f);
            ImGui::SetNextItemWidth(half);
            const float lowTime = index > 0 ? gradient.ColorKeys[index - 1].Time : 0.0f;
            const float highTime = index + 1 < static_cast<int>(gradient.ColorKeyCount) ? gradient.ColorKeys[index + 1].Time : 1.0f;
            changed |= ImGui::DragFloat("##KeyTime", &key.Time, 0.005f, lowTime, highTime, "t %.3f", ImGuiSliderFlags_AlwaysClamp);
        }
        else if(kind == 2)
        {
            ParticleGradient::AlphaKey& key = gradient.AlphaKeys[index];
            const float half = (ImGui::GetContentRegionAvail().x - 4.0f) * 0.5f;
            ImGui::SetNextItemWidth(half);
            changed |= ImGui::SliderFloat("##KeyAlpha", &key.Alpha, 0.0f, 1.0f, "alpha %.2f");
            ImGui::SameLine(0.0f, 4.0f);
            ImGui::SetNextItemWidth(half);
            const float lowTime = index > 0 ? gradient.AlphaKeys[index - 1].Time : 0.0f;
            const float highTime = index + 1 < static_cast<int>(gradient.AlphaKeyCount) ? gradient.AlphaKeys[index + 1].Time : 1.0f;
            changed |= ImGui::DragFloat("##KeyTime", &key.Time, 0.005f, lowTime, highTime, "t %.3f", ImGuiSliderFlags_AlwaysClamp);
        }

        storage->SetInt(kindId, kind);
        storage->SetInt(indexId, index);
        storage->SetBool(dragId, dragging);
        ImGui::PopID();
        return changed;
    }

    //////////////////////////////////////////////////////////////////////////
    // Table rows
    //////////////////////////////////////////////////////////////////////////

    bool FloatRow(const char* label, ParticleFloat& value, float speed, float min, float max, const char* format)
    {
        static constexpr const char* modeNames[] = { "Constant", "Random Between Two Constants", "Curve", "Random Between Two Curves" };
        bool changed = false;
        ImGui::PushID(label);
        EditorUI::TLabel(label);

        switch(value.Mode)
        {
            case ParticleValueMode::CONSTANT:
                ImGui::SetNextItemWidth(EditorUI::ModeButtonValueWidth());
                changed |= ImGui::DragFloat("##Value", &value.Max, speed, min, max, format);
                break;
            case ParticleValueMode::RANDOM_BETWEEN_CONSTANTS:
            {
                const float half = (EditorUI::ModeButtonValueWidth() - 2.0f) * 0.5f;
                ImGui::SetNextItemWidth(half);
                changed |= ImGui::DragFloat("##Min", &value.Min, speed, min, max, format);
                ImGui::SameLine(0.0f, 2.0f);
                ImGui::SetNextItemWidth(half);
                changed |= ImGui::DragFloat("##Max", &value.Max, speed, min, max, format);
                break;
            }
            case ParticleValueMode::CURVE:
            case ParticleValueMode::RANDOM_BETWEEN_CURVES:
                ImGui::SetNextItemWidth(EditorUI::ModeButtonValueWidth());
                changed |= ImGui::DragFloat("##Multiplier", &value.CurveMultiplier, speed, 0.0f, 0.0f, "x %.2f");
                break;
        }

        const int picked = EditorUI::ModeButton(modeNames, 4, static_cast<int>(value.Mode));
        if(picked >= 0)
        {
            const ParticleValueMode mode = static_cast<ParticleValueMode>(picked);
            if((mode == ParticleValueMode::CURVE || mode == ParticleValueMode::RANDOM_BETWEEN_CURVES) &&
               (value.Mode == ParticleValueMode::CONSTANT || value.Mode == ParticleValueMode::RANDOM_BETWEEN_CONSTANTS))
            {
                // The curves start flat at 1, the multiplier carries the old value
                value.CurveMultiplier = value.Max;
                value.CurveMin.SetConstant(value.Max != 0.0f ? value.Min / value.Max : 1.0f);
                value.CurveMax.SetConstant(1.0f);
            }
            else if(mode == ParticleValueMode::RANDOM_BETWEEN_CONSTANTS && value.Mode == ParticleValueMode::CONSTANT)
                value.Min = value.Max;
            value.Mode = mode;
            changed = true;
        }

        if(value.Mode == ParticleValueMode::CURVE)
            changed |= CurveEditor("##Curve", value.CurveMax);
        else if(value.Mode == ParticleValueMode::RANDOM_BETWEEN_CURVES)
        {
            ImGui::TextDisabled("Min");
            changed |= CurveEditor("##CurveMin", value.CurveMin, 48.0f);
            ImGui::TextDisabled("Max");
            changed |= CurveEditor("##CurveMax", value.CurveMax, 48.0f);
        }

        ImGui::PopID();
        return changed;
    }

    bool ColorRow(const char* label, ParticleColor& color)
    {
        static constexpr const char* modeNames[] = { "Color", "Random Between Two Colors", "Gradient (over the duration)", "Random Color In Gradient" };
        bool changed = false;
        ImGui::PushID(label);
        EditorUI::TLabel(label);

        constexpr ImGuiColorEditFlags flags = ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs;
        switch(color.Mode)
        {
            case ParticleColorMode::CONSTANT:
                ImGui::SetNextItemWidth(EditorUI::ModeButtonValueWidth());
                changed |= ImGui::ColorEdit4("##Color", glm::value_ptr(color.ColorA), flags);
                break;
            case ParticleColorMode::RANDOM_BETWEEN_COLORS:
                changed |= ImGui::ColorEdit4("##ColorA", glm::value_ptr(color.ColorA), flags);
                ImGui::SameLine(0.0f, 4.0f);
                changed |= ImGui::ColorEdit4("##ColorB", glm::value_ptr(color.ColorB), flags);
                break;
            case ParticleColorMode::GRADIENT:
            case ParticleColorMode::RANDOM_IN_GRADIENT:
                ImGui::TextDisabled("%s", color.Mode == ParticleColorMode::GRADIENT ? "Gradient over the duration" : "Random point in the gradient");
                break;
        }

        const int picked = EditorUI::ModeButton(modeNames, 4, static_cast<int>(color.Mode));
        if(picked >= 0)
        {
            color.Mode = static_cast<ParticleColorMode>(picked);
            changed = true;
        }

        if(color.Mode == ParticleColorMode::GRADIENT || color.Mode == ParticleColorMode::RANDOM_IN_GRADIENT)
            changed |= GradientEditor("##Gradient", color.Gradient);

        ImGui::PopID();
        return changed;
    }

    bool CurveRow(const char* label, ParticleCurve& curve)
    {
        ImGui::PushID(label);
        EditorUI::TLabel(label);
        const bool changed = CurveEditor("##Curve", curve);
        ImGui::PopID();
        return changed;
    }

    bool GradientRow(const char* label, ParticleGradient& gradient)
    {
        ImGui::PushID(label);
        EditorUI::TLabel(label);
        const bool changed = GradientEditor("##Gradient", gradient);
        ImGui::PopID();
        return changed;
    }

} // namespace Surge::ParticleWidgets
