// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include <imgui.h>

namespace Surge
{
    class Entity;

    enum class GizmoIcon
    {
        CAMERA,
        POINT_LIGHT,
        DIRECTIONAL_LIGHT,
        AUDIO_SOURCE,
        AUDIO_LISTENER,
        ENVIRONMENT,

        // Scene Hierarchy entity types
        ENTITY,
        MESH,
        SPRITE,
        TEXT,
        UI_CANVAS,
        SCRIPT,
        PARTICLES,

        // Inspector component headers
        TRANSFORM,
        RIGIDBODY,
        COLLIDER,
        LINK,

        // Title bar playback glyphs
        PLAY,
        STOP,
        PAUSE,
        STEP,

        ADD
    };

    // Vector glyphs built from ImDrawList primitives, crisp at any size & no texture assets to ship (pogchamp)
    namespace GizmoIcons
    {
        struct BadgeStyle
        {
            ImU32 Background;
            ImU32 Rim;
            float RimThickness;
            ImU32 Shadow;
        };

        // A round badge (drop shadow, background, rim) with the glyph centered inside it
        void DrawBadge(ImDrawList* drawList, const ImVec2& center, float radius, GizmoIcon icon, ImU32 glyphColor, const BadgeStyle& style);

        // The glyph alone, fitted into a size x size square around center. cutoutColor is used for small details carved into the glyph
        void DrawGlyph(ImDrawList* drawList, const ImVec2& center, float size, GizmoIcon icon, ImU32 color, ImU32 cutoutColor);

        // An ImGui button showing the glyph: background from the current ImGuiCol_Button* colors and FrameRounding,
        // glyph fitted into glyphScale of the button's smaller side. Honors BeginDisabled() (the colors go through the style alpha)
        bool IconButton(const char* strId, GizmoIcon icon, const ImVec2& size, const ImVec4& glyphColor, float glyphScale = 0.62f);

        // Same button with a texture instead of a glyph (white icons, tinted by 'tint'), the image is a square of iconScale of the smaller side
        bool IconButton(const char* strId, ImTextureID texture, const ImVec2& size, const ImVec4& tint, float iconScale = 0.8f);

        struct EntityIcon
        {
            GizmoIcon Icon;
            ImU32 Color;
        };

        // The glyph of the component that best describes the entity (Scene Hierarchy rows, Inspector header)
        EntityIcon GetEntityIcon(Entity entity);

    } // namespace GizmoIcons

} // namespace Surge
