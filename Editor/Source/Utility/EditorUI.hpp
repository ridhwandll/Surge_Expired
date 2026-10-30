// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <array>
#include <functional>
#include <imgui.h>
#include <imgui_internal.h>
#include <cstdarg>

#define IMGUI_ENTITY_PAYLOAD "ENTITY_PAYLOAD" // Hierarchy drag & drop, payload: entt::entity of the Hierarchy's scene

// The Editor's ImGui toolkit: palette, fonts, scoped styles and widgets. Function name prefixed with T means it is used inside ImGui::BeginTable
namespace Surge::EditorUI
{
    // 8 bits per channel (IM_COL32 values) as an ImVec4, converts back to the same ImU32
    consteval ImVec4 Rgba8(int r, int g, int b, int a = 255) { return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f); }
    constexpr ImVec4 WithAlpha(const ImVec4& color, float alpha) { return ImVec4(color.x, color.y, color.z, alpha); }
    constexpr ImVec4 Scale(const ImVec4& color, float factor) { return ImVec4(color.x * factor, color.y * factor, color.z * factor, color.w); } // Darkens the rgb, keeps the alpha
    constexpr ImVec4 Mix(const ImVec4& a, const ImVec4& b, float t) { return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t); }

    // ImGui::ColorConvertFloat4ToU32, usable in constant expressions. Unlike ImGui::GetColorU32 it ignores the style alpha (disabled items)
    constexpr ImU32 ToU32(const ImVec4& color)
    {
        auto channel = [](float v) { return static_cast<ImU32>((v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v)) * 255.0f + 0.5f); };
        return IM_COL32(channel(color.x), channel(color.y), channel(color.z), channel(color.w));
    }
    constexpr ImU32 WithAlpha(ImU32 color, float alpha) { return (color & ~IM_COL32_A_MASK) | (static_cast<ImU32>(alpha * 255.0f + 0.5f) << IM_COL32_A_SHIFT); }
    inline ImU32 ScaleAlpha(ImU32 color, float factor) { return (color & ~IM_COL32_A_MASK) | (static_cast<ImU32>(static_cast<float>((color >> IM_COL32_A_SHIFT) & 0xFF) * glm::clamp(factor, 0.0f, 1.0f)) << IM_COL32_A_SHIFT); }

    enum class Theme
    {
        DARK,
        LIGHT
    };

    // Rewrites the themed colors and ImGui's style colors, takes effect on the next drawn item
    void SetTheme(Theme theme);
    Theme GetTheme();

    // Every color the Editor draws with. Panels never write color literals, they pick (or derive with WithAlpha/Scale/Mix) one of these
    namespace Colors
    {
        constexpr ImVec4 Transparent = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        constexpr ImVec4 Black = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
        constexpr ImVec4 White = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        constexpr ImVec4 Gray(float v) { return ImVec4(v, v, v, 1.0f); } // Brightness v (0..1)

        // Brand: the same in every theme
        constexpr ImVec4 Accent = ImVec4(1.0f, 0.647f, 0.0f, 1.0f); // Amber: titles, selections, the play mode bar
        constexpr ImVec4 AccentLight = ImVec4(1.0f, 0.74f, 0.25f, 1.0f);
        constexpr ImVec4 AccentDark = ImVec4(0.85f, 0.55f, 0.0f, 1.0f);
        constexpr ImVec4 AccentMuted = ImVec4(0.42f, 0.33f, 0.13f, 1.0f);
        constexpr ImVec4 AccentPale = ImVec4(1.0f, 0.95f, 0.8f, 1.0f); // Play mode sweep
        constexpr ImVec4 OnAccent = ImVec4(0.06f, 0.045f, 0.02f, 1.0f);  // Text on accent buttons
        constexpr ImVec4 Gold = ImVec4(1.0f, 0.766f, 0.336f, 1.0f);
        constexpr ImVec4 Curve = Rgba8(255, 191, 0);
        constexpr ImVec4 Highlight = Rgba8(255, 200, 80); // Yellow markers: UI editor anchors and its viewport
        constexpr ImVec4 WarningDark = ImVec4(0.8f, 0.6f, 0.2f, 1.0f);
        constexpr ImVec4 Orange = ImVec4(0.8f, 0.5f, 0.2f, 1.0f);
        constexpr ImVec4 Danger = ImVec4(0.8f, 0.2f, 0.2f, 1.0f); // Destructive buttons
        constexpr ImVec4 ErrorMuted = ImVec4(0.65f, 0.25f, 0.25f, 1.0f);
        constexpr ImVec4 ErrorDark = ImVec4(0.55f, 0.18f, 0.18f, 1.0f);
        constexpr ImVec4 Crimson = ImVec4(0.6f, 0.05f, 0.08f, 1.0f); // Errors on the amber play mode bar
        constexpr ImVec4 CloseHover = ImVec4(0.77f, 0.17f, 0.11f, 1.0f); // Windows 11 caption close button
        constexpr ImVec4 Blue = Rgba8(100, 160, 255); // GPU timings, UI editor hover and pivot

        // Themed: written by SetTheme, the dark values until then. Read them at draw time, never into constexpr/static constants
        // Neutrals, ToneNN = NN% brightness in the dark theme, mirrored (dark <-> light) in the light theme
        inline ImVec4 Tone04 = Gray(0.04f);
        inline ImVec4 Tone05 = Gray(0.05f);
        inline ImVec4 Tone06 = Gray(0.06f);
        inline ImVec4 Tone07 = Gray(0.07f);
        inline ImVec4 Tone09 = Gray(0.09f);
        inline ImVec4 Tone10 = Gray(0.1f);
        inline ImVec4 Tone11 = Gray(0.11f);
        inline ImVec4 Tone12 = Gray(0.12f);
        inline ImVec4 Tone15 = Gray(0.15f);
        inline ImVec4 Tone16 = Gray(0.16f);
        inline ImVec4 Tone18 = Gray(0.18f);
        inline ImVec4 Tone19 = Gray(0.19f);
        inline ImVec4 Tone20 = Gray(0.2f);
        inline ImVec4 Tone22 = Gray(0.22f);
        inline ImVec4 Tone25 = Gray(0.25f);
        inline ImVec4 Tone28 = Gray(0.28f);
        inline ImVec4 Tone30 = Gray(0.3f);
        inline ImVec4 Tone34 = Gray(0.34f);
        inline ImVec4 Tone35 = Gray(0.35f);
        inline ImVec4 Tone38 = Gray(0.38f);
        inline ImVec4 Tone40 = Gray(0.4f);
        inline ImVec4 Tone43 = Gray(0.43f);
        inline ImVec4 Tone45 = Gray(0.45f);
        inline ImVec4 Tone47 = Gray(0.47f);
        inline ImVec4 Tone50 = Gray(0.5f);
        inline ImVec4 Tone55 = Gray(0.55f);
        inline ImVec4 Tone60 = Gray(0.6f);
        inline ImVec4 Tone62 = Gray(0.62f);
        inline ImVec4 Tone67 = Gray(0.67f);
        inline ImVec4 Tone70 = Gray(0.7f);
        inline ImVec4 Tone73 = Gray(0.73f);
        inline ImVec4 Tone78 = Gray(0.78f);
        inline ImVec4 Tone80 = Gray(0.8f);
        inline ImVec4 Tone82 = Gray(0.82f);
        inline ImVec4 Tone85 = Gray(0.85f);
        inline ImVec4 Tone86 = Gray(0.86f);
        inline ImVec4 Tone88 = Gray(0.88f);
        inline ImVec4 Tone90 = Gray(0.9f);
        inline ImVec4 Tone92 = Gray(0.92f);
        inline ImVec4 Tone94 = Gray(0.94f);
        inline ImVec4 Tone96 = Gray(0.96f);

        inline ImVec4 Ink = White; // Highest contrast: hover tints, active glyphs, text on Selection (black in the light theme)
        inline ImVec4 Selection = Gray(0.3f); // Selected rows, toggled buttons, focus outlines
        inline ImVec4 Background = Gray(0.05f); // Title bar, status bar
        inline ImVec4 Text = Gray(0.9f);
        inline ImVec4 TextBright = Gray(0.92f);
        inline ImVec4 TextMuted = Gray(0.55f);
        inline ImVec4 Hint = Gray(0.5f); // Menu headers, secondary labels
        inline ImVec4 PopupBackground = Gray(0.11f);
        inline ImVec4 PopupBorder = Gray(0.22f);
        inline ImVec4 Border = Gray(0.16f); // Lines between the bars and the dockspace
        inline ImVec4 Overlay = WithAlpha(Gray(0.06f), 0.78f); // Toolbars floating over viewports

        // Themed status and text tints, darker in the light theme to stay readable
        inline ImVec4 Success = ImVec4(0.4f, 0.85f, 0.5f, 1.0f);
        inline ImVec4 Warning = ImVec4(1.0f, 0.8f, 0.25f, 1.0f);
        inline ImVec4 Error = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
        inline ImVec4 ErrorSoft = ImVec4(0.85f, 0.45f, 0.45f, 1.0f); // Text of missing references
        inline ImVec4 Fatal = ImVec4(1.0f, 0.1f, 0.1f, 1.0f);
        inline ImVec4 Info = ImVec4(0.4f, 0.8f, 0.9f, 1.0f);
        inline ImVec4 HeatHigh = ImVec4(1.0f, 0.6f, 0.35f, 1.0f);
        inline ImVec4 HeatMedium = ImVec4(1.0f, 0.85f, 0.5f, 1.0f);
        inline ImVec4 AccentSoft = ImVec4(0.94f, 0.84f, 0.54f, 1.0f); // Text of notice banners
        inline ImVec4 AccentDeep = Rgba8(72, 52, 28); // Selected render graph pass
        inline ImVec4 ErrorDeep = Rgba8(62, 28, 28);  // Disabled render graph pass

        // Drawn inside rendered scenes and previews (gizmo labels, ImGuizmo, preview stats): the same in every theme
        namespace Viewport
        {
            constexpr ImVec4 Text = Gray(0.92f);
            constexpr ImVec4 TextDim = Gray(0.78f);
            constexpr ImVec4 TextFaint = Gray(0.5f);
            constexpr ImVec4 Inactive = Gray(0.6f);
            constexpr ImVec4 Shade = Gray(0.06f); // Label backgrounds
            constexpr ImVec4 Clear = Gray(0.04f); // Behind previews until they render
        } // namespace Viewport

        // Axes (gizmos, transform fields)
        constexpr ImVec4 AxisX = ImVec4(0.984f, 0.2f, 0.2f, 1.0f);
        constexpr ImVec4 AxisY = ImVec4(0.553f, 0.831f, 0.0f, 1.0f);
        constexpr ImVec4 AxisZ = ImVec4(0.137f, 0.565f, 1.0f, 1.0f);

        // Viewport gizmos
        namespace Gizmo
        {
            constexpr ImVec4 AudioRange = Rgba8(120, 205, 255);
            constexpr ImVec4 AudioRangeInner = Rgba8(70, 140, 235);
            constexpr ImVec4 Skeleton = Rgba8(190, 165, 255);
            constexpr ImVec4 EnvironmentIcon = Rgba8(255, 200, 110);
            constexpr ImVec4 ParticleCollision = Rgba8(90, 220, 200);
            constexpr ImVec4 ParticleBounds = Rgba8(255, 210, 90);
            constexpr ImVec4 IconBackground = Rgba8(26, 26, 30);
            constexpr ImVec4 IconBackgroundHovered = Rgba8(58, 58, 64);
            constexpr ImVec4 LineRim = WithAlpha(Black, 0.27f); // Dark rim under every wire, keeps them readable on bright backgrounds
        } // namespace Gizmo

        // Components: Inspector headers, Add Component menu, gizmo icons
        namespace Component
        {
            constexpr ImVec4 Transform = Rgba8(205, 205, 205);
            constexpr ImVec4 Camera = Rgba8(130, 190, 255);
            constexpr ImVec4 Sprite = Rgba8(255, 145, 95);
            constexpr ImVec4 Mesh = Rgba8(150, 160, 255);
            constexpr ImVec4 Light = Rgba8(255, 205, 90);
            constexpr ImVec4 Environment = Rgba8(255, 170, 100);
            constexpr ImVec4 RendererSettings = Rgba8(120, 200, 230);
            constexpr ImVec4 Text = Rgba8(225, 225, 225);
            constexpr ImVec4 Physics = Rgba8(255, 125, 120);
            constexpr ImVec4 Collider = Rgba8(120, 215, 130);
            constexpr ImVec4 Script = Rgba8(230, 220, 80);
            constexpr ImVec4 UICanvas = Rgba8(240, 130, 190);
            constexpr ImVec4 Audio = Rgba8(110, 215, 160);
            constexpr ImVec4 Particles = Rgba8(90, 220, 245);
            constexpr ImVec4 Animator = Rgba8(170, 140, 255);
            constexpr ImVec4 Entity = Rgba8(150, 150, 150);
        } // namespace Component

        // Render graph pass groups: Renderer panel timings, Render Graph view. Fixed per group, a group keeps its color in every view and session
        namespace PassGroup
        {
            constexpr ImVec4 Shadow = Rgba8(146, 112, 202);
            constexpr ImVec4 MainScene = Rgba8(91, 143, 249);
            constexpr ImVec4 AmbientOcclusion = Rgba8(90, 216, 166);
            constexpr ImVec4 Bloom = Rgba8(246, 189, 22);
            constexpr ImVec4 PostProcess = Rgba8(232, 104, 74);
            constexpr ImVec4 UIOverlay = Rgba8(109, 200, 236);
            constexpr ImVec4 Swapchain = Rgba8(255, 157, 77);
            constexpr ImVec4 Unknown = Rgba8(115, 125, 115);
        } // namespace PassGroup

        // Asset types: Content Browser tiles, type bars and filters
        namespace Asset
        {
            constexpr ImVec4 Texture = ImVec4(0.9f, 0.36f, 0.36f, 1.0f);
            constexpr ImVec4 Sprite = ImVec4(0.95f, 0.5f, 0.7f, 1.0f);
            constexpr ImVec4 Mesh = ImVec4(0.3f, 0.7f, 0.98f, 1.0f);
            constexpr ImVec4 Material = ImVec4(0.42f, 0.82f, 0.38f, 1.0f);
            constexpr ImVec4 Scene = ImVec4(1.0f, 0.62f, 0.18f, 1.0f);
            constexpr ImVec4 Script = ImVec4(0.96f, 0.84f, 0.3f, 1.0f);
            constexpr ImVec4 Font = Gray(0.85f);
            constexpr ImVec4 Audio = ImVec4(0.3f, 0.86f, 0.76f, 1.0f);
            constexpr ImVec4 UILayout = ImVec4(0.42f, 0.58f, 1.0f, 1.0f);
            constexpr ImVec4 ParticleEffect = ImVec4(0.35f, 0.86f, 0.96f, 1.0f);
            constexpr ImVec4 Unknown = Gray(0.45f);
            constexpr ImVec4 Folder = ImVec4(0.86f, 0.7f, 0.36f, 1.0f);
        } // namespace Asset
    } // namespace Colors

    // Console lines, status bar messages
    inline ImVec4 SeverityColor(Severity severity)
    {
        switch(severity)
        {
            case Severity::Trace: return Colors::Tone60;
            case Severity::Info:  return Colors::Success;
            case Severity::Debug: return Colors::Info;
            case Severity::Warn:  return Colors::Warning;
            case Severity::Error: return Colors::Error;
            case Severity::Fatal: return Colors::Fatal;
        }
        return Colors::White;
    }

    // -------------------- Fonts and scoped styles
    // Fonts loaded by VulkanImGui.cpp: 0 = regular, 1 = bold
    inline ImFont* RegularFont() { return ImGui::GetIO().Fonts->Fonts[0]; }
    inline ImFont* BoldFont() { return ImGui::GetIO().Fonts->Fonts[1]; }

    struct ScopedBoldFont
    {
        SURGE_DISABLE_COPY_AND_MOVE(ScopedBoldFont);
        ScopedBoldFont() { ImGui::PushFont(BoldFont()); }
        explicit ScopedBoldFont(float fontSize) { ImGui::PushFont(BoldFont(), fontSize); }
        ~ScopedBoldFont() { ImGui::PopFont(); }
    };

    struct ScopedColor
    {
        int Size;
        SURGE_DISABLE_COPY_AND_MOVE(ScopedColor);
        ScopedColor(const std::initializer_list<ImGuiCol>& ids, const ImVec4& col)
            : Size(static_cast<int>(ids.size()))
        {
            for(auto& id : ids)
                ImGui::PushStyleColor(id, col);
        }
        ~ScopedColor() { ImGui::PopStyleColor(Size); }
    };

    // Solid button: hovered/active shades derived from color
    struct ScopedButtonColor
    {
        SURGE_DISABLE_COPY_AND_MOVE(ScopedButtonColor);
        ScopedButtonColor(const ImVec4& color, const ImVec4& textColor)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, color);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Mix(color, Colors::White, 0.12f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, Scale(color, 0.85f));
            ImGui::PushStyleColor(ImGuiCol_Text, textColor);
        }
        ~ScopedButtonColor() { ImGui::PopStyleColor(4); }
    };

    // Values of the aligned T* rows (TDragFloat & co) are drawn in the bold font while one of these lives, like the Inspector's TProperty
    struct ScopedBoldValues
    {
        SURGE_DISABLE_COPY_AND_MOVE(ScopedBoldValues);
        ScopedBoldValues();
        ~ScopedBoldValues();
    };

    // -------------------- Draw lists and text
    void DrawRectAroundWidget(const ImVec4& color, float thickness = 1.5f, float rounding = 1.0f);
    void TextCentered(const char* text);
    void TextLinkOpenURLCentered(const char* text, const char* url);
    // Draw list text on whole pixels: ImGui bakes glyphs at the rounded size, fractional positions blur them
    void AddTextSnapped(ImDrawList* drawList, ImFont* font, float fontSize, float x, float y, ImU32 color, const char* text);
    void AddTextCentered(ImDrawList* drawList, ImFont* font, float fontSize, float centerX, float y, ImU32 color, const char* text);
    // Pixel rect ImGui draws without anti-aliasing, stays sharp
    void AddPixelRect(ImDrawList* drawList, float x, float y, float width, float height, ImU32 color);
    // Darkens towards the right edge of a side panel (folder tree, project list sidebar)
    void AddSidePanelShadow(ImDrawList* drawList, const ImVec2& min, const ImVec2& max);
    // Up to two lines centered in width, the last one ends with "..." when the text doesn't fit
    void AddWrappedLabel(ImDrawList* drawList, const String& text, ImVec2 pos, float width, ImU32 color);
    // Small rounded bold label, centered on centerX. anchorBottom: y is its bottom edge, otherwise its top. The font shrinks to fit maxWidth
    void AddFittedBadge(ImDrawList* drawList, float centerX, float y, bool anchorBottom, float maxWidth, const char* text, ImU32 background, ImU32 textColor);
    // Full width bar ending at bottomY, bold label centered, the font shrinks to fit narrow bars
    void AddLabelBar(ImDrawList* drawList, float minX, float maxX, float bottomY, const char* text, ImU32 background, ImU32 textColor);
    // Left to right gradient
    void AddHorizontalGradient(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, ImU32 left, ImU32 right);
    // Rounded tint, strongest (the color's alpha) on the left edge (right edge when fromRight), fading to nothing at endFraction of the width (0..1)
    void AddFadingRect(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, float rounding, ImU32 color, float endFraction = 1.0f, bool fromRight = false);
    // ImGui has no rotated text: lays the text out at layoutPos (must be visible, ImGui culls glyphs outside the clip rect), then turns it 90 degrees
    // counter clockwise around bottomLeft so it reads upwards
    void AddTextRotated(ImDrawList* drawList, const ImVec2& layoutPos, const ImVec2& bottomLeft, ImU32 color, const char* text);
    // Bold accent title over a separator, spaced from the content above unless first
    void SectionHeading(const char* title, bool first = false);
    // Bold text on the left, muted text right aligned on the same line
    void SplitText(const ImVec4& leftColor, const String& left, const String& right);
    // Rounded bar, progress < 0 = indeterminate (a chunk sweeping across). sheen: a light runs over the filled part
    void AddProgressBar(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, float progress, const ImVec4& fillColor, const ImVec4& trackColor, bool sheen = false);
    // Bold, accent colored separator with a title
    void SeparatorTitle(const char* title, float spacingBefore = 0.0f);
    // Bold gray title of a menu / popup section, followed by a StyledSeparator
    void MenuHeader(const char* title);
    // "(?)" after the last item, its text shown as a tooltip
    void HelpMarker(const char* text);

    // -------------------- Tooltips, popups and menus
    // Tooltip for the last item, shown once the mouse rests on it for a moment (disabled items too), padded and styled like the popups
    void ItemTooltip(const char* text);
    // Begin/EndTooltip with ItemTooltip's style, for tooltips with more than one line of text. wrapWidth < 0: no wrapping
    bool BeginStyledTooltip(ImGuiHoveredFlags hoveredFlags = ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_Stationary | ImGuiHoveredFlags_AllowWhenDisabled, float wrapWidth = -1.0f);
    void EndStyledTooltip();
    inline void DelayedToolTip(const char* tip)
    {
        if(ImGui::IsItemHovered() && GImGui->HoveredIdTimer > 0.5f)
        {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(tip);
            ImGui::EndTooltip();
        }
    }

    struct StyledPopupVars
    {
        static void Push();
        static void Pop();
    };

    inline bool BeginStyledPopupContextItem(const char* strID = nullptr, ImGuiPopupFlags popupFlags = 1)
    {
        StyledPopupVars::Push();
        if(ImGui::BeginPopupContextItem(strID, popupFlags))
            return true;
        StyledPopupVars::Pop();
        return false;
    }

    inline bool BeginStyledPopupContextWindow(const char* strID = nullptr, ImGuiPopupFlags popupFlags = 1 | 2)
    {
        StyledPopupVars::Push();
        if(ImGui::BeginPopupContextWindow(strID, popupFlags))
            return true;
        StyledPopupVars::Pop();
        return false;
    }

    inline void EndStyledPopup()
    {
        ImGui::EndPopup();
        StyledPopupVars::Pop();
    }

    inline void StyledSeparator()
    {
        ScopedColor color({ ImGuiCol_Separator }, Colors::Accent);
        ImGui::Separator();
    }

    inline bool StyledMenuItem(const char* label, const char* shortcut = nullptr, bool* p_selected = nullptr, bool enabled = true)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));
        const bool clicked = ImGui::MenuItem(label, shortcut, p_selected, enabled);
        ImGui::PopStyleVar();
        return clicked;
    }

    // -------------------- Widgets
    inline bool Button(const char* label, const ImVec2& size = ImVec2(0, 0))
    {
        const bool result = ImGui::Button(label, size);
        if(ImGui::IsItemHovered() || ImGui::IsItemActive())
            DrawRectAroundWidget(Colors::Selection, 2.0f, 1.0f);
        return result;
    }

    inline void ShiftCursorX(float x) { ImGui::SetCursorPosX(ImGui::GetCursorPosX() + x); }

    // On/off button tinted with color while on, returns true when flipped
    bool ColoredToggle(const char* label, bool& value, const ImVec4& color);

    // Small "v" button on the line of the last item, opens the list of names. Returns the picked index or -1
    int ModeButton(const char* const* names, int count, int current);
    // Width left for a value followed by a ModeButton
    inline float ModeButtonValueWidth() { return ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - 2.0f; }

    // Bold button on the input background, for actions next to text and fields
    bool FlatButton(const char* label);
    // InputInt without step buttons, clamped to [min, max]
    void InputIntClamped(const char* id, int& value, int min, int max, float width);
    // Read only path field + BROWSE (+ X to clear when clearable) filling the line, browse() returns the picked path or an empty string
    bool PathField(const char* id, String& value, const char* hint, bool clearable, const std::function<String()>& browse);
    // Full width progress bar at the cursor
    void ProgressBar(float progress, float height, const ImVec4& fillColor, const ImVec4& trackColor, bool sheen = false);

    // Custom drawn buttons of the Project Browser, labels and states stay consistent
    enum class ButtonStyle
    {
        PRIMARY,
        SECONDARY,
        GHOST
    };
    constexpr float CONTROL_HEIGHT = 36.0f;
    float ActionButtonWidth(const char* label);
    bool ActionButton(const char* id, const char* label, ButtonStyle style, const ImVec2& size, bool enabled = true);
    // Vertical three dots button (component options)
    bool KebabButton(const char* id, float size);
    // "..." button, three pixel squares
    bool MoreButton(const char* id, float size, bool menuOpen);
    float BadgeWidth(const char* text);
    float AddBadge(ImDrawList* drawList, const ImVec2& pos, const char* text, const ImVec4& color); // Returns the width taken
    // Small bold caption above a field
    void FieldLabel(const char* text);
    // Accent outline around the last item while it is active (text fields)
    void FocusRing();

    // Tall rounded fields with a border (CONTROL_HEIGHT)
    struct ScopedInputStyle
    {
        SURGE_DISABLE_COPY_AND_MOVE(ScopedInputStyle);
        ScopedInputStyle();
        ~ScopedInputStyle();
    };

    // -------------------- Form rows (no table): a muted label, then the value widget from valueX
    void FormLabel(const char* label, float valueX);
    bool FormInputText(const char* label, const char* id, String& value, float valueX, const char* hint = "", ImGuiInputTextFlags flags = 0);
    bool FormCheckbox(const char* label, const char* id, bool& value, float valueX, const char* help);

    // -------------------- Property tables (T*)
    enum class CustomPropertyFlag
    {
        None,
        Color3,
        Color4,
    };

    // Two column property tables: a fixed width name column, then the value column
    bool BeginPropertyTable(const char* id, float labelWidth, ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit);

    inline void TSeparator(const char* text, bool disabled = false)
    {
        ScopedBoldFont font;
        ScopedColor color({ ImGuiCol_Separator }, disabled ? Colors::Tone92 : Colors::Accent);
        ImGui::TableNextColumn();
        ImGui::SeparatorText(text);
        ImGui::TableNextColumn();
    }

    // Collapsible section title spanning both columns, ImGui keeps the open state per ID
    bool TSection(const char* title);

    inline void TString(const char* title, const char* fmt, ...)
    {
        ImGui::PushID(title);
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(title);
        ImGui::TableNextColumn();

        va_list args;
        va_start(args, fmt);
        ImGui::TextWrappedV(fmt, args);
        va_end(args);

        ImGui::PopID();
    }

    // Inspector style rows: bold value, outlined while hovered
    template <typename T, CustomPropertyFlag F = CustomPropertyFlag::None>
    bool TProperty(const char* title, T* value, float dragMin = 0.0f, float dragMax = 0.0f)
    {
        ImGui::PushID(title);
        bool result = false;
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(title);
        ImGui::TableNextColumn();
        ImGui::PushItemWidth(-1);
        {
            ScopedBoldFont font;
            if constexpr(F == CustomPropertyFlag::None)
            {
                if constexpr(std::is_same_v<T, int>)
                    result = ImGui::DragInt("##v", value, 1, static_cast<int>(dragMin), static_cast<int>(dragMax));
                else if constexpr(std::is_same_v<T, float>)
                    result = ImGui::DragFloat("##v", value, 0.01, dragMin, dragMax);
                else if constexpr(std::is_same_v<T, glm::vec2>)
                    result = ImGui::DragFloat2("##v", glm::value_ptr(*value), 0.01f, dragMin, dragMax, "%.2f");
                else if constexpr(std::is_same_v<T, glm::vec3>)
                    result = ImGui::DragFloat3("##v", glm::value_ptr(*value), 0.01f, dragMin, dragMax, "%.2f");
                else if constexpr(std::is_same_v<T, glm::vec4>)
                    result = ImGui::DragFloat4("##v", glm::value_ptr(*value), 0.01f, dragMin, dragMax, "%.2f");
                else if constexpr(std::is_same_v<T, bool>)
                    result = ImGui::Checkbox("##v", value);
                else
                    static_assert(false);
            }
            else if constexpr(F == CustomPropertyFlag::Color3 && std::is_same_v<T, glm::vec3>)
                result = ImGui::ColorEdit3("##v", glm::value_ptr(*value));
            else if constexpr(F == CustomPropertyFlag::Color4 && std::is_same_v<T, glm::vec4>)
                result = ImGui::ColorEdit4("##v", glm::value_ptr(*value), ImGuiColorEditFlags_None | ImGuiColorEditFlags_AlphaBar);
            else
                static_assert(false, "Invalid case! Maybe you used wrong CustomPropertyFlag with wrong type? For example: Using glm::vec3 with CustomPropertyFlag::Color4");
        }

        ImGui::PopItemWidth();
        if(ImGui::IsItemHovered() || ImGui::IsItemActive())
            DrawRectAroundWidget(Colors::Selection, 2.0f, 1.0f);
        ImGui::PopID();

        return result;
    }

    template <typename T>
    bool TSlider(const char* label, T* value, T min, T max, const char* format = "%.3f")
    {
        bool modified = false;
        ImGui::PushID(label);
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(label);
        ImGui::TableNextColumn();
        ImGui::PushItemWidth(-1);
        {
            ScopedBoldFont font;
            if constexpr(std::is_same_v<T, float>)
                modified = ImGui::SliderFloat("##label", value, min, max, format);
            else if constexpr(std::is_same_v<T, int>)
                modified = ImGui::SliderInt("##label", value, min, max);
            else
                static_assert(false);
        }
        ImGui::PopItemWidth();
        ImGui::PopID();
        return modified;
    }

    template <typename T, size_t N>
    bool TComboBox(const char* label, T& currentValue, const std::array<const char*, N>& options)
    {
        bool modified = false;
        const size_t currentIndex = static_cast<size_t>(currentValue);
        const char* currentString = (currentIndex < N) ? options[currentIndex] : "UNKNOWN THIS SHOULD NOT HAPPEN!";

        ImGui::TableNextColumn();
        ImGui::TextUnformatted(label);
        ImGui::TableNextColumn();
        ImGui::PushItemWidth(-1);

        ImGui::PushID(label);
        if(ImGui::BeginCombo("##Combo", currentString))
        {
            for(size_t i = 0; i < N; i++)
            {
                const bool isSelected = (currentIndex == i);
                if(ImGui::Selectable(options[i], isSelected))
                {
                    currentValue = static_cast<T>(i);
                    modified = true;
                }
                if(isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::PopID();
        ImGui::PopItemWidth();
        return modified;
    }

    // Aligned rows: the name cell is aligned to the frame of the value widget, which fills the value cell. All return true when the value changed.
    // Drags clamp (typed input too) once they have a range (min or max not 0)

    // Name cell, then moves to the value cell. The next item fills it unless the caller sizes it
    void TLabel(const char* label, const char* tooltip = nullptr);
    // Wrapped text in the value cell of its own row, under the row it explains. fontSize 0: the current font, label: name cell text (in color)
    void THint(const char* text, const ImVec4& color = Colors::TextMuted, float fontSize = 13.0f, const char* label = nullptr);
    // X/Y/Z fields behind colored axis tags (click: reset that axis), optional uniform lock (scale)
    bool TVec3Control(const char* label, glm::vec3& values, float resetValue, float speed, bool* uniformLock = nullptr);
    bool TDragFloat(const char* label, float* value, float speed, float min = 0.0f, float max = 0.0f, const char* format = "%.2f");
    bool TSliderFloat(const char* label, float* value, float min, float max, const char* format = "%.2f");
    bool TDragInt(const char* label, int* value, float speed, int min = 0, int max = 0);
    bool TSliderInt(const char* label, int* value, int min, int max);
    bool TDragFloatN(const char* label, float* values, int components, float speed, float min = 0.0f, float max = 0.0f, const char* format = "%.2f");
    bool TCheckbox(const char* label, bool* value);
    bool TColorEdit3(const char* label, float* rgb, ImGuiColorEditFlags flags = 0);
    bool TColorEdit4(const char* label, float* rgba, ImGuiColorEditFlags flags = ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf);
    bool TInputText(const char* label, String& value, bool multiline = false);
    bool TCombo(const char* label, int* index, const char* const* items, int count);

    template <typename V>
    bool TDragVec(const char* label, V& value, float speed, float min = 0.0f, float max = 0.0f, const char* format = "%.2f")
    {
        return TDragFloatN(label, glm::value_ptr(value), static_cast<int>(V::length()), speed, min, max, format);
    }

    // Any integer type, clamped to [min, max]
    template <typename T>
    bool TDragInteger(const char* label, T* value, float speed, int min, int max)
    {
        int edited = static_cast<int>(std::min<T>(*value, static_cast<T>(max)));
        if(!TDragInt(label, &edited, speed, min, max))
            return false;
        *value = static_cast<T>(edited);
        return true;
    }

    // Combo over a list of values with their names
    template <typename E, size_t N>
    bool TComboValues(const char* label, E& value, const std::array<const char*, N>& names, const std::array<E, N>& values)
    {
        int index = 0;
        for(size_t i = 0; i < N; i++)
        {
            if(values[i] == value)
                index = static_cast<int>(i);
        }
        if(!TCombo(label, &index, names.data(), static_cast<int>(N)))
            return false;
        value = values[index];
        return true;
    }

    // Enum combo, names indexed by the enum value
    template <typename T, size_t N>
    bool TComboEnum(const char* label, T& value, const std::array<const char*, N>& names)
    {
        int index = static_cast<int>(value);
        if(!TCombo(label, &index, names.data(), static_cast<int>(N)) || index == static_cast<int>(value))
            return false;
        value = static_cast<T>(index);
        return true;
    }

    // -------------------- Editor frame
    // buildLayout(dockspaceID, size) runs right before the dockspace is submitted when rebuildLayout is set or no layout exists yet (fresh imgui.ini)
    // bottomBarHeight: space kept free under the dockspace (status bar)
    void DockSpace(float titleBarHeight, const std::function<void(ImGuiID, ImVec2)>& buildLayout = nullptr, bool rebuildLayout = false, float bottomBarHeight = 0.0f);
    void ShowConfirmationBox(const String& title, const String& message, std::function<void()> onConfirm, std::function<void()> onCancel = nullptr);
    void RenderConfirmationBox();

} // namespace Surge::EditorUI
