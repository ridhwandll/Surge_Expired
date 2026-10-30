// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Utility/EditorUI.hpp"
#include "Gizmos/GizmoIcons.hpp"
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <cmath>

namespace Surge
{
    namespace
    {
        struct ConfirmationState
        {
            bool IsOpen = false;
            String Title;
            String Message;
            std::function<void()> OnConfirm;
            std::function<void()> OnCancel;
        };
        ConfirmationState sConfirmState;
        int sBoldValues = 0;
        EditorUI::Theme sTheme = EditorUI::Theme::DARK;

        // Light theme gray for a dark theme one: dark backgrounds become light ones and light text dark, a little short of pure black/white
        constexpr float MirrorTone(float dark) { return 0.97f - dark * 0.94f; }

        // Whole pixels only: ImGui bakes glyphs at the rounded size and scales the quads to a fractional one, which blurs the text
        constexpr float BUTTON_FONT_SIZE = 16.0f;
        constexpr float CAPTION_FONT_SIZE = 13.0f;
        constexpr float BADGE_FONT_SIZE = 12.0f;
        constexpr float BUTTON_ROUNDING = 2.0f;

        void PushTooltipStyle()
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 4.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 4.0f)); // Callers may have none pushed (title bar, status bar)
            ImGui::PushStyleColor(ImGuiCol_PopupBg, EditorUI::WithAlpha(EditorUI::Colors::PopupBackground, 0.97f));
            ImGui::PushStyleColor(ImGuiCol_Border, EditorUI::Colors::PopupBorder);
            ImGui::PushStyleColor(ImGuiCol_Text, EditorUI::Colors::Text); // Callers may have a tinted text color pushed (title bar in play mode)
        }

        void PopTooltipStyle()
        {
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar(4);
        }

        // Bold font size that fits text + padding into maxWidth (15 px, no smaller than 8)
        float FitBadgeFont(const char* text, float maxWidth, float paddingX, ImVec2& outTextSize)
        {
            ImFont* font = EditorUI::BoldFont();
            float fontSize = 15.0f;
            outTextSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);
            if(outTextSize.x + paddingX * 2.0f > maxWidth && outTextSize.x > 0.0f)
            {
                fontSize = std::max(8.0f, fontSize * (maxWidth - paddingX * 2.0f) / outTextSize.x);
                outTextSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);
            }
            return fontSize;
        }

        // Value widgets of the aligned rows, between PushID(label) + TLabel and PopID
        struct ValueScope
        {
            ValueScope(const char* label)
            {
                ImGui::PushID(label);
                EditorUI::TLabel(label);
                if(sBoldValues > 0)
                    ImGui::PushFont(EditorUI::BoldFont());
            }
            ~ValueScope()
            {
                if(sBoldValues > 0)
                    ImGui::PopFont();
                ImGui::PopID();
            }
        };
    } // namespace

    void EditorUI::SetTheme(Theme theme)
    {
        sTheme = theme;
        const bool light = theme == Theme::LIGHT;
        auto tone = [light](float dark) { return Colors::Gray(light ? MirrorTone(dark) : dark); };
        auto pick = [light](const ImVec4& dark, const ImVec4& lightColor) { return light ? lightColor : dark; };

        struct ToneSlot
        {
            ImVec4* Color;
            float Dark;
        };
        const ToneSlot tones[] = {
                { &Colors::Tone04, 0.04f },
                { &Colors::Tone05, 0.05f },
                { &Colors::Tone06, 0.06f },
                { &Colors::Tone07, 0.07f },
                { &Colors::Tone09, 0.09f },
                { &Colors::Tone10, 0.1f },
                { &Colors::Tone11, 0.11f },
                { &Colors::Tone12, 0.12f },
                { &Colors::Tone15, 0.15f },
                { &Colors::Tone16, 0.16f },
                { &Colors::Tone18, 0.18f },
                { &Colors::Tone19, 0.19f },
                { &Colors::Tone20, 0.2f },
                { &Colors::Tone22, 0.22f },
                { &Colors::Tone25, 0.25f },
                { &Colors::Tone28, 0.28f },
                { &Colors::Tone30, 0.3f },
                { &Colors::Tone34, 0.34f },
                { &Colors::Tone35, 0.35f },
                { &Colors::Tone38, 0.38f },
                { &Colors::Tone40, 0.4f },
                { &Colors::Tone43, 0.43f },
                { &Colors::Tone45, 0.45f },
                { &Colors::Tone47, 0.47f },
                { &Colors::Tone50, 0.5f },
                { &Colors::Tone55, 0.55f },
                { &Colors::Tone60, 0.6f },
                { &Colors::Tone62, 0.62f },
                { &Colors::Tone67, 0.67f },
                { &Colors::Tone70, 0.7f },
                { &Colors::Tone73, 0.73f },
                { &Colors::Tone78, 0.78f },
                { &Colors::Tone80, 0.8f },
                { &Colors::Tone82, 0.82f },
                { &Colors::Tone85, 0.85f },
                { &Colors::Tone86, 0.86f },
                { &Colors::Tone88, 0.88f },
                { &Colors::Tone90, 0.9f },
                { &Colors::Tone92, 0.92f },
                { &Colors::Tone94, 0.94f },
                { &Colors::Tone96, 0.96f },
        };
        for(const ToneSlot& slot : tones)
            *slot.Color = tone(slot.Dark);

        Colors::Ink = light ? Colors::Black : Colors::White;
        Colors::Selection = tone(0.3f);
        Colors::Background = tone(0.05f);
        Colors::Text = tone(0.9f);
        Colors::TextBright = tone(0.92f);
        Colors::TextMuted = tone(0.55f);
        Colors::Hint = tone(0.5f);
        Colors::PopupBackground = tone(0.11f);
        Colors::PopupBorder = tone(0.22f);
        Colors::Border = tone(0.16f);
        Colors::Overlay = WithAlpha(tone(0.06f), 0.78f);

        Colors::Success = pick(ImVec4(0.4f, 0.85f, 0.5f, 1.0f), ImVec4(0.13f, 0.55f, 0.25f, 1.0f));
        Colors::Warning = pick(ImVec4(1.0f, 0.8f, 0.25f, 1.0f), ImVec4(0.7f, 0.47f, 0.0f, 1.0f));
        Colors::Error = pick(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), ImVec4(0.8f, 0.18f, 0.18f, 1.0f));
        Colors::ErrorSoft = pick(ImVec4(0.85f, 0.45f, 0.45f, 1.0f), ImVec4(0.7f, 0.25f, 0.25f, 1.0f));
        Colors::Fatal = pick(ImVec4(1.0f, 0.1f, 0.1f, 1.0f), ImVec4(0.7f, 0.0f, 0.0f, 1.0f));
        Colors::Info = pick(ImVec4(0.4f, 0.8f, 0.9f, 1.0f), ImVec4(0.05f, 0.45f, 0.6f, 1.0f));
        Colors::HeatHigh = pick(ImVec4(1.0f, 0.6f, 0.35f, 1.0f), ImVec4(0.85f, 0.35f, 0.05f, 1.0f));
        Colors::HeatMedium = pick(ImVec4(1.0f, 0.85f, 0.5f, 1.0f), ImVec4(0.7f, 0.5f, 0.05f, 1.0f));
        Colors::AccentSoft = pick(ImVec4(0.94f, 0.84f, 0.54f, 1.0f), ImVec4(0.55f, 0.38f, 0.0f, 1.0f));
        Colors::AccentDeep = pick(Rgba8(72, 52, 28), Rgba8(255, 225, 180));
        Colors::ErrorDeep = pick(Rgba8(62, 28, 28), Rgba8(245, 210, 210));

        // Dark set is what VulkanImGui.cpp sets up, the light one mirrors its grays over ImGui's light style
        if(light)
            ImGui::StyleColorsLight();
        else
            ImGui::StyleColorsDark();

        ImVec4* colors = ImGui::GetStyle().Colors;
        auto gray = [&tone](int byte) { return tone(static_cast<float>(byte) / 255.0f); };
        colors[ImGuiCol_Header] = gray(51);
        colors[ImGuiCol_HeaderHovered] = WithAlpha(tone(0.3f), 0.3f);
        colors[ImGuiCol_HeaderActive] = gray(22);
        colors[ImGuiCol_CheckMark] = gray(190);
        colors[ImGuiCol_Button] = gray(40);
        colors[ImGuiCol_ButtonHovered] = gray(120);
        colors[ImGuiCol_ButtonActive] = gray(10);
        colors[ImGuiCol_SeparatorHovered] = ImVec4(0.8f, 0.4f, 0.1f, 1.0f);
        colors[ImGuiCol_SeparatorActive] = ImVec4(1.0f, 0.5f, 0.1f, 1.0f);
        colors[ImGuiCol_FrameBg] = gray(10);
        colors[ImGuiCol_FrameBgHovered] = gray(35);
        colors[ImGuiCol_FrameBgActive] = gray(80);
        colors[ImGuiCol_Tab] = gray(56);
        colors[ImGuiCol_TabHovered] = gray(70);
        colors[ImGuiCol_TabActive] = gray(90);
        colors[ImGuiCol_TabUnfocused] = gray(40);
        colors[ImGuiCol_TabDimmedSelected] = gray(88);
        colors[ImGuiCol_WindowBg] = gray(25);
        colors[ImGuiCol_PopupBg] = gray(45);
        colors[ImGuiCol_DockingPreview] = gray(10);
        colors[ImGuiCol_TitleBg] = tone(0.12f);
        colors[ImGuiCol_TitleBgActive] = tone(0.14f);
        colors[ImGuiCol_ScrollbarBg] = gray(10);
        colors[ImGuiCol_ScrollbarGrabActive] = gray(220);
        colors[ImGuiCol_SliderGrab] = gray(190);
        colors[ImGuiCol_SliderGrabActive] = gray(220);
        if(light)
        {
            colors[ImGuiCol_Text] = tone(1.0f);
            colors[ImGuiCol_MenuBarBg] = gray(25);
        }
    }

    EditorUI::Theme EditorUI::GetTheme()
    {
        return sTheme;
    }

    EditorUI::ScopedBoldValues::ScopedBoldValues() { sBoldValues++; }
    EditorUI::ScopedBoldValues::~ScopedBoldValues() { sBoldValues--; }

    void EditorUI::DrawRectAroundWidget(const ImVec4& color, float thickness, float rounding)
    {
        ImGuiContext& g = *GImGui;
        const ImRect& rect = (g.LastItemData.StatusFlags & ImGuiItemStatusFlags_HasDisplayRect) ? g.LastItemData.DisplayRect : g.LastItemData.Rect;
        ImGui::GetWindowDrawList()->AddRect(rect.Min, rect.Max, ImGui::ColorConvertFloat4ToU32(color), rounding, ImDrawFlags_RoundCornersAll, thickness);
    }

    void EditorUI::TextCentered(const char* text)
    {
        const float windowWidth = ImGui::GetWindowSize().x;
        const float textWidth = ImGui::CalcTextSize(text).x;
        ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
        ImGui::TextUnformatted(text);
    }

    void EditorUI::TextLinkOpenURLCentered(const char* text, const char* url)
    {
        const float windowWidth = ImGui::GetWindowSize().x;
        const float textWidth = ImGui::CalcTextSize(text).x;
        ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
        ImGui::TextLinkOpenURL(text, url);
    }

    void EditorUI::AddTextSnapped(ImDrawList* drawList, ImFont* font, float fontSize, float x, float y, ImU32 color, const char* text)
    {
        drawList->AddText(font, fontSize, ImVec2(std::floor(x), std::floor(y)), color, text);
    }

    void EditorUI::AddTextCentered(ImDrawList* drawList, ImFont* font, float fontSize, float centerX, float y, ImU32 color, const char* text)
    {
        const float width = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text).x;
        AddTextSnapped(drawList, font, fontSize, centerX - width * 0.5f, y, color, text);
    }

    void EditorUI::AddPixelRect(ImDrawList* drawList, float x, float y, float width, float height, ImU32 color)
    {
        const float left = std::floor(x);
        const float top = std::floor(y);
        drawList->AddRectFilled(ImVec2(left, top), ImVec2(left + width, top + height), color);
    }

    void EditorUI::AddSidePanelShadow(ImDrawList* drawList, const ImVec2& min, const ImVec2& max)
    {
        AddHorizontalGradient(drawList, min, max, ToU32(WithAlpha(Colors::Black, 0.04f)), ToU32(WithAlpha(Colors::Black, 0.78f)));
    }

    void EditorUI::AddWrappedLabel(ImDrawList* drawList, const String& text, ImVec2 pos, float width, ImU32 color)
    {
        constexpr int maxLines = 2;
        const float lineHeight = ImGui::GetTextLineHeight();
        const char* cursor = text.c_str();
        const char* end = cursor + text.size();

        for(int line = 0; line < maxLines && cursor < end; line++)
        {
            const bool lastLine = line == maxLines - 1;
            const char* lineEnd = end;
            if(ImGui::CalcTextSize(cursor, end).x > width)
            {
                // Longest prefix that fits, preferably broken after a separator
                const float available = lastLine ? width - ImGui::CalcTextSize("...").x : width;
                lineEnd = cursor;
                while(lineEnd < end)
                {
                    const char* next = lineEnd + 1;
                    while(next < end && (static_cast<unsigned char>(*next) & 0xC0) == 0x80)
                        next++; // Whole UTF-8 sequences
                    if(ImGui::CalcTextSize(cursor, next).x > available)
                        break;
                    lineEnd = next;
                }
                if(lineEnd == cursor)
                    lineEnd = cursor + 1;

                if(!lastLine)
                {
                    for(const char* breakAt = lineEnd; breakAt > cursor + 1; breakAt--)
                    {
                        const char c = *(breakAt - 1);
                        if(c == ' ' || c == '_' || c == '-' || c == '.')
                        {
                            lineEnd = breakAt;
                            break;
                        }
                    }
                }
            }

            String lineText(cursor, lineEnd);
            if(lastLine && lineEnd < end)
                lineText += "...";
            const float lineWidth = ImGui::CalcTextSize(lineText.c_str()).x;
            drawList->AddText(ImVec2(pos.x + std::max(0.0f, (width - lineWidth) * 0.5f), pos.y), color, lineText.c_str());

            cursor = lineEnd;
            while(cursor < end && *cursor == ' ')
                cursor++;
            pos.y += lineHeight;
        }
    }

    void EditorUI::AddFittedBadge(ImDrawList* drawList, float centerX, float y, bool anchorBottom, float maxWidth, const char* text, ImU32 background, ImU32 textColor)
    {
        constexpr float paddingX = 4.0f;
        constexpr float paddingY = 2.0f;
        ImVec2 textSize;
        const float fontSize = FitBadgeFont(text, maxWidth, paddingX, textSize);

        const ImVec2 badgeSize(textSize.x + paddingX * 2.0f, textSize.y + paddingY * 2.0f);
        const ImVec2 min(std::floor(centerX - badgeSize.x * 0.5f), std::floor(anchorBottom ? y - badgeSize.y : y));
        drawList->AddRectFilled(min, ImVec2(min.x + badgeSize.x, min.y + badgeSize.y), background, 3.0f);
        drawList->AddText(BoldFont(), fontSize, ImVec2(min.x + paddingX, min.y + paddingY), textColor, text);
    }

    void EditorUI::AddLabelBar(ImDrawList* drawList, float minX, float maxX, float bottomY, const char* text, ImU32 background, ImU32 textColor)
    {
        constexpr float paddingX = 4.0f;
        constexpr float paddingY = 2.0f;
        const float width = maxX - minX;
        ImVec2 textSize;
        const float fontSize = FitBadgeFont(text, width, paddingX, textSize);

        const float top = std::floor(bottomY - textSize.y - paddingY * 2.0f);
        drawList->AddRectFilled(ImVec2(minX, top), ImVec2(maxX, bottomY), background, 4.0f, ImDrawFlags_RoundCornersBottom);
        drawList->AddText(BoldFont(), fontSize, ImVec2(std::floor(minX + (width - textSize.x) * 0.5f), top + paddingY), textColor, text);
    }

    void EditorUI::AddHorizontalGradient(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, ImU32 left, ImU32 right)
    {
        drawList->AddRectFilledMultiColor(min, max, left, right, right, left);
    }

    void EditorUI::AddFadingRect(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, float rounding, ImU32 color, float endFraction, bool fromRight)
    {
        const int firstVertex = drawList->VtxBuffer.Size;
        drawList->AddRectFilled(min, max, color, rounding);

        const float invWidth = 1.0f / std::max((max.x - min.x) * endFraction, 1.0f);
        for(int i = firstVertex; i < drawList->VtxBuffer.Size; i++)
        {
            ImDrawVert& vertex = drawList->VtxBuffer[i];
            const float distance = fromRight ? max.x - vertex.pos.x : vertex.pos.x - min.x;
            const float fade = 1.0f - std::clamp(distance * invWidth, 0.0f, 1.0f);
            const ImU32 alpha = static_cast<ImU32>(static_cast<float>(vertex.col >> IM_COL32_A_SHIFT) * fade);
            vertex.col = (vertex.col & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT); // The anti-aliased fringe keeps its zero alpha
        }
    }

    void EditorUI::AddTextRotated(ImDrawList* drawList, const ImVec2& layoutPos, const ImVec2& bottomLeft, ImU32 color, const char* text)
    {
        const int first = drawList->VtxBuffer.Size;
        drawList->AddText(layoutPos, color, text);
        for(int i = first; i < drawList->VtxBuffer.Size; i++)
        {
            ImVec2& pos = drawList->VtxBuffer[i].pos;
            const ImVec2 offset(pos.x - layoutPos.x, pos.y - layoutPos.y);
            pos = ImVec2(bottomLeft.x + offset.y, bottomLeft.y - offset.x);
        }
    }

    void EditorUI::SectionHeading(const char* title, bool first)
    {
        if(!first)
            ImGui::Dummy(ImVec2(0.0f, 14.0f));
        {
            ScopedBoldFont font;
            ImGui::TextColored(Colors::Accent, "%s", title);
        }
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
    }

    void EditorUI::SplitText(const ImVec4& leftColor, const String& left, const String& right)
    {
        {
            ScopedBoldFont font(16.0f);
            ImGui::TextColored(leftColor, "%s", left.c_str());
        }
        if(!right.empty())
        {
            ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::CalcTextSize(right.c_str()).x);
            ImGui::TextColored(Colors::Tone67, "%s", right.c_str());
        }
    }

    void EditorUI::AddProgressBar(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, float progress, const ImVec4& fillColor, const ImVec4& trackColor, bool sheen)
    {
        const float rounding = (max.y - min.y) * 0.5f;
        drawList->AddRectFilled(min, max, ImGui::GetColorU32(trackColor), rounding);

        const ImU32 fill = ImGui::GetColorU32(fillColor);
        const float width = max.x - min.x;
        const float time = static_cast<float>(ImGui::GetTime());
        if(progress < 0.0f)
        {
            // Indeterminate: a chunk sweeping across the bar
            constexpr float chunk = 0.3f;
            const float t = std::fmod(time * 0.8f, 1.0f + chunk) - chunk;
            const float begin = min.x + std::max(0.0f, t) * width;
            const float end = min.x + std::min(1.0f, t + chunk) * width;
            if(end > begin)
                drawList->AddRectFilled(ImVec2(begin, min.y), ImVec2(end, max.y), fill, rounding);
            return;
        }
        if(progress <= 0.0f)
            return;

        const float fillEnd = min.x + std::max(width * std::min(progress, 1.0f), rounding * 2.0f);
        drawList->AddRectFilled(min, ImVec2(fillEnd, max.y), fill, rounding);
        if(!sheen)
            return;

        // Sheen running over the filled part, so long steps still look alive
        constexpr float sheenWidth = 60.0f;
        const ImU32 clear = ToU32(WithAlpha(Colors::White, 0.0f));
        const ImU32 bright = ToU32(WithAlpha(Colors::White, 70.0f / 255.0f));
        const float sheenX = min.x - sheenWidth + std::fmod(time * 220.0f, (fillEnd - min.x) + sheenWidth * 2.0f);
        drawList->PushClipRect(min, ImVec2(fillEnd, max.y), true);
        drawList->AddRectFilledMultiColor(ImVec2(sheenX, min.y), ImVec2(sheenX + sheenWidth * 0.5f, max.y), clear, bright, bright, clear);
        drawList->AddRectFilledMultiColor(ImVec2(sheenX + sheenWidth * 0.5f, min.y), ImVec2(sheenX + sheenWidth, max.y), bright, clear, clear, bright);
        drawList->PopClipRect();
    }

    void EditorUI::SeparatorTitle(const char* title, float spacingBefore)
    {
        if(spacingBefore > 0.0f)
            ImGui::Dummy(ImVec2(0.0f, spacingBefore));
        ScopedBoldFont font;
        ScopedColor color({ ImGuiCol_Separator }, Colors::Accent);
        ImGui::SeparatorText(title);
    }

    void EditorUI::MenuHeader(const char* title)
    {
        {
            ScopedBoldFont font;
            ImGui::TextColored(Colors::Hint, "%s", title);
        }
        StyledSeparator();
    }

    void EditorUI::HelpMarker(const char* text)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if(ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", text);
    }

    void EditorUI::ItemTooltip(const char* text)
    {
        if(!BeginStyledTooltip())
            return;
        ImGui::TextUnformatted(text);
        EndStyledTooltip();
    }

    bool EditorUI::BeginStyledTooltip(ImGuiHoveredFlags hoveredFlags, float wrapWidth)
    {
        if(!ImGui::IsItemHovered(hoveredFlags))
            return false;

        PushTooltipStyle();
        if(ImGui::BeginTooltip())
        {
            ImGui::PushTextWrapPos(wrapWidth);
            return true;
        }
        PopTooltipStyle();
        return false;
    }

    void EditorUI::EndStyledTooltip()
    {
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
        PopTooltipStyle();
    }

    void EditorUI::StyledPopupVars::Push()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);

        ImGui::PushStyleColor(ImGuiCol_PopupBg, Colors::PopupBackground);
        ImGui::PushStyleColor(ImGuiCol_Border, Colors::Tone20);
        ImGui::PushStyleColor(ImGuiCol_Header, Colors::Tone25);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, Colors::Tone28);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, Colors::Tone20);
        ImGui::PushStyleColor(ImGuiCol_Text, Colors::Text);
    }

    void EditorUI::StyledPopupVars::Pop()
    {
        ImGui::PopStyleVar(4);
        ImGui::PopStyleColor(6);
    }

    bool EditorUI::ColoredToggle(const char* label, bool& value, const ImVec4& color)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, value ? Scale(color, 0.35f) : Colors::Tone10);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Scale(color, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_Text, value ? color : Colors::Tone50);
        const bool clicked = ImGui::Button(label);
        ImGui::PopStyleColor(3);
        if(clicked)
            value = !value;
        return clicked;
    }

    int EditorUI::ModeButton(const char* const* names, int count, int current)
    {
        int picked = -1;
        ImGui::SameLine(0.0f, 2.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
        if(ImGui::ArrowButton("##Mode", ImGuiDir_Down))
            ImGui::OpenPopup("##ModePopup");
        ImGui::PopStyleColor();
        DelayedToolTip(names[current]);

        StyledPopupVars::Push();
        if(ImGui::BeginPopup("##ModePopup"))
        {
            for(int i = 0; i < count; i++)
            {
                if(StyledMenuItem(names[i], nullptr, nullptr, true) && i != current)
                    picked = i;
            }
            EndStyledPopup();
        }
        else
            StyledPopupVars::Pop();
        return picked;
    }

    bool EditorUI::FlatButton(const char* label)
    {
        ScopedBoldFont font;
        ImGui::PushStyleColor(ImGuiCol_Button, Colors::Tone07);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Colors::Tone10);
        const bool pressed = Button(label);
        ImGui::PopStyleColor(2);
        return pressed;
    }

    void EditorUI::InputIntClamped(const char* id, int& value, int min, int max, float width)
    {
        ImGui::SetNextItemWidth(width);
        if(ImGui::InputInt(id, &value, 0, 0))
            value = std::clamp(value, min, max);
    }

    bool EditorUI::PathField(const char* id, String& value, const char* hint, bool clearable, const std::function<String()>& browse)
    {
        ImGui::PushID(id);
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        float browseWidth, clearWidth;
        {
            ScopedBoldFont font;
            browseWidth = ImGui::CalcTextSize("BROWSE").x + 28.0f;
            clearWidth = ImGui::CalcTextSize("X").x + 28.0f;
        }

        const bool showClear = clearable && !value.empty();
        ImGui::SetNextItemWidth(-(browseWidth + spacing + (showClear ? clearWidth + spacing : 0.0f)));
        ImGui::InputTextWithHint("##path", hint, &value, ImGuiInputTextFlags_ReadOnly);

        bool changed = false;
        ScopedBoldFont font;
        ScopedColor color({ ImGuiCol_Button }, Colors::Tone07);
        ImGui::SameLine();
        if(Button("BROWSE", ImVec2(browseWidth, 0.0f)))
        {
            String selected = browse();
            if(!selected.empty())
            {
                value = std::move(selected);
                changed = true;
            }
        }
        if(showClear)
        {
            ImGui::SameLine();
            if(Button("X", ImVec2(clearWidth, 0.0f)))
            {
                value.clear();
                changed = true;
            }
        }
        ImGui::PopID();
        return changed;
    }

    void EditorUI::ProgressBar(float progress, float height, const ImVec4& fillColor, const ImVec4& trackColor, bool sheen)
    {
        const ImVec2 min = ImGui::GetCursorScreenPos();
        const float width = ImGui::GetContentRegionAvail().x;
        AddProgressBar(ImGui::GetWindowDrawList(), min, ImVec2(min.x + width, min.y + height), progress, fillColor, trackColor, sheen);
        ImGui::Dummy(ImVec2(width, height));
    }

    float EditorUI::ActionButtonWidth(const char* label)
    {
        return std::ceil(BoldFont()->CalcTextSizeA(BUTTON_FONT_SIZE, FLT_MAX, 0.0f, label).x + 32.0f);
    }

    bool EditorUI::ActionButton(const char* id, const char* label, ButtonStyle style, const ImVec2& size, bool enabled)
    {
        const ImVec2 min = ImGui::GetCursorScreenPos();
        const ImVec2 max(min.x + size.x, min.y + size.y);

        ImGui::BeginDisabled(!enabled);
        const bool pressed = ImGui::InvisibleButton(id, size);
        ImGui::EndDisabled();
        const bool hovered = ImGui::IsItemHovered();
        const bool held = ImGui::IsItemActive();

        ImVec4 background = Colors::Transparent;
        ImVec4 text = Colors::TextBright;
        switch(style)
        {
            case ButtonStyle::PRIMARY:
                background = !enabled ? Colors::Tone12 : (hovered && !held ? Colors::AccentLight : Colors::Accent);
                text = enabled ? Colors::OnAccent : Colors::Tone34;
                break;
            case ButtonStyle::SECONDARY:
                background = held ? Colors::Tone19 : (hovered ? Colors::Tone16 : Colors::Tone12);
                text = enabled ? Colors::TextBright : Colors::Tone34;
                break;
            case ButtonStyle::GHOST:
                background = held ? Colors::Tone19 : (hovered ? Colors::Tone12 : background);
                text = !enabled ? Colors::Tone34 : (hovered ? Colors::TextBright : Colors::TextMuted);
                break;
        }

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(min, max, ToU32(background), BUTTON_ROUNDING);
        if(style == ButtonStyle::SECONDARY)
            drawList->AddRect(min, max, ToU32(Colors::Border), BUTTON_ROUNDING);

        const ImVec2 textSize = BoldFont()->CalcTextSizeA(BUTTON_FONT_SIZE, FLT_MAX, 0.0f, label);
        AddTextSnapped(drawList, BoldFont(), BUTTON_FONT_SIZE, min.x + (size.x - textSize.x) * 0.5f, min.y + (size.y - textSize.y) * 0.5f, ToU32(text), label);
        return pressed;
    }

    bool EditorUI::KebabButton(const char* id, float size)
    {
        const bool pressed = ImGui::InvisibleButton(id, ImVec2(size, size));
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        if(ImGui::IsItemHovered())
            drawList->AddRectFilled(min, max, ToU32(WithAlpha(Colors::Ink, 0.09f)), ImGui::GetStyle().FrameRounding);

        const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
        const float gap = size * 0.18f;
        const float radius = std::max(1.5f, size * 0.06f);
        const ImU32 color = ImGui::GetColorU32(ImGui::IsItemHovered() ? ImGuiCol_Text : ImGuiCol_FrameBg);
        for(int i = -1; i <= 1; i++)
            drawList->AddCircleFilled(ImVec2(center.x, center.y + gap * static_cast<float>(i)), radius, color, 8);
        return pressed;
    }

    bool EditorUI::MoreButton(const char* id, float size, bool menuOpen)
    {
        const ImVec2 min = ImGui::GetCursorScreenPos();
        const bool pressed = ImGui::InvisibleButton(id, ImVec2(size, size));
        const bool hovered = ImGui::IsItemHovered();

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        if(hovered || menuOpen)
            drawList->AddRectFilled(min, ImVec2(min.x + size, min.y + size), ToU32(ImGui::IsItemActive() || menuOpen ? Colors::Tone19 : Colors::Tone16), BUTTON_ROUNDING);

        const ImU32 color = ToU32(hovered || menuOpen ? Colors::TextBright : Colors::TextMuted);
        const float centerX = std::floor(min.x + size * 0.5f);
        const float centerY = std::floor(min.y + size * 0.5f);
        for(int i = -1; i <= 1; i++)
            AddPixelRect(drawList, centerX - 1.0f + static_cast<float>(i) * 6.0f, centerY - 1.0f, 3.0f, 3.0f, color);
        return pressed;
    }

    float EditorUI::BadgeWidth(const char* text)
    {
        return std::ceil(BoldFont()->CalcTextSizeA(BADGE_FONT_SIZE, FLT_MAX, 0.0f, text).x) + 12.0f;
    }

    float EditorUI::AddBadge(ImDrawList* drawList, const ImVec2& pos, const char* text, const ImVec4& color)
    {
        AddTextSnapped(drawList, BoldFont(), BADGE_FONT_SIZE, std::floor(pos.x) + 6.0f, std::floor(pos.y) + 3.0f, ToU32(color), text);
        return BadgeWidth(text);
    }

    void EditorUI::FieldLabel(const char* text)
    {
        ScopedBoldFont font(CAPTION_FONT_SIZE);
        ImGui::TextColored(Colors::TextMuted, "%s", text);
    }

    void EditorUI::FocusRing()
    {
        if(!ImGui::IsItemActive())
            return;

        ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ToU32(Colors::Accent), 6.0f, 0, 1.0f);
        ImGui::SetNavCursorVisible(false); // The ring already marks the field, no second (nav) rectangle around it
    }

    EditorUI::ScopedInputStyle::ScopedInputStyle()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, std::floor((CONTROL_HEIGHT - ImGui::GetFontSize()) * 0.5f)));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, Colors::Tone12);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, Colors::Tone16);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, Colors::Tone16);
        ImGui::PushStyleColor(ImGuiCol_Border, Colors::Border);
        ImGui::PushStyleColor(ImGuiCol_TextDisabled, Colors::Tone34); // Hint text
    }

    EditorUI::ScopedInputStyle::~ScopedInputStyle()
    {
        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar(3);
    }

    void EditorUI::FormLabel(const char* label, float valueX)
    {
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(Colors::Tone67, "%s", label);
        ImGui::SameLine(valueX);
    }

    bool EditorUI::FormInputText(const char* label, const char* id, String& value, float valueX, const char* hint, ImGuiInputTextFlags flags)
    {
        FormLabel(label, valueX);
        ImGui::SetNextItemWidth(-FLT_MIN);
        return ImGui::InputTextWithHint(id, hint, &value, flags);
    }

    bool EditorUI::FormCheckbox(const char* label, const char* id, bool& value, float valueX, const char* help)
    {
        FormLabel(label, valueX);
        const bool changed = ImGui::Checkbox(id, &value);
        HelpMarker(help);
        return changed;
    }

    bool EditorUI::BeginPropertyTable(const char* id, float labelWidth, ImGuiTableFlags flags)
    {
        if(!ImGui::BeginTable(id, 2, flags))
            return false;
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, labelWidth);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        return true;
    }

    bool EditorUI::TSection(const char* title)
    {
        ScopedBoldFont font;
        ImGui::TableNextColumn();
        const bool open = ImGui::TreeNodeEx(title, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAllColumns | ImGuiTreeNodeFlags_NoTreePushOnOpen);
        ImGui::TableNextColumn();
        return open;
    }

    void EditorUI::TLabel(const char* label, const char* tooltip)
    {
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        if(tooltip)
            DelayedToolTip(tooltip);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
    }

    void EditorUI::THint(const char* text, const ImVec4& color, float fontSize, const char* label)
    {
        ImGui::TableNextColumn();
        if(label)
            ImGui::TextColored(color, "%s", label);
        ImGui::TableNextColumn();
        ImGui::PushTextWrapPos(0.0f);
        if(fontSize > 0.0f)
            ImGui::PushFont(RegularFont(), fontSize);
        ImGui::TextColored(color, "%s", text);
        if(fontSize > 0.0f)
            ImGui::PopFont();
        ImGui::PopTextWrapPos();
    }

    bool EditorUI::TVec3Control(const char* label, glm::vec3& values, float resetValue, float speed, bool* uniformLock)
    {
        bool modified = false;
        ImGui::PushID(label);

        const float frameHeight = ImGui::GetFrameHeight();
        const float spacing = 4.0f;
        const bool locked = uniformLock && *uniformLock;

        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);

        if(uniformLock)
        {
            ImGui::SameLine(0.0f, spacing);
            const ImVec4 glyphColor = locked ? Colors::Accent : Colors::TextMuted;
            if(GizmoIcons::IconButton("##UniformLock", GizmoIcon::LINK, ImVec2(frameHeight, frameHeight), glyphColor, 0.66f))
                *uniformLock = !*uniformLock;
            DelayedToolTip(locked ? "Uniform scale: ON" : "Uniform scale: OFF");
        }
        ImGui::TableNextColumn();

        const float fieldWidth = glm::max((ImGui::GetContentRegionAvail().x - spacing * 2.0f) / 3.0f, 30.0f);
        const float tagWidth = glm::min(frameHeight * 0.85f, fieldWidth * 0.4f);
        const float rounding = ImGui::GetStyle().FrameRounding;
        const glm::vec3 before = values;

        constexpr const char* axisNames[] = { "X", "Y", "Z" };
        constexpr ImU32 axisColors[] = { ToU32(Scale(Colors::AxisX, 0.8f)), ToU32(Scale(Colors::AxisY, 0.8f)), ToU32(Scale(Colors::AxisZ, 0.8f)) };

        int changedAxis = -1;
        bool wasReset = false;
        for(int axis = 0; axis < 3; axis++)
        {
            ImGui::PushID(axis);

            // Axis tag
            if(ImGui::InvisibleButton("##Tag", ImVec2(tagWidth, frameHeight)))
            {
                values[axis] = resetValue;
                changedAxis = axis;
                wasReset = true;
            }
            DelayedToolTip("Click to reset");
            const ImVec2 tagMin = ImGui::GetItemRectMin();
            const ImVec2 tagMax = ImGui::GetItemRectMax();
            const ImU32 tagColor = ImGui::IsItemHovered() ? WithAlpha(axisColors[axis], 0.85f) : axisColors[axis];
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(tagMin, tagMax, tagColor, rounding, ImDrawFlags_RoundCornersLeft);
            {
                ScopedBoldFont font;
                const ImVec2 textSize = ImGui::CalcTextSize(axisNames[axis]);
                drawList->AddText(ImVec2((tagMin.x + tagMax.x - textSize.x) * 0.5f, (tagMin.y + tagMax.y - textSize.y) * 0.5f), ToU32(Colors::White), axisNames[axis]);
            }

            // Value
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::SetNextItemWidth(fieldWidth - tagWidth);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
            if(ImGui::DragFloat("##Value", &values[axis], speed, 0.0f, 0.0f, "%.3f", ImGuiSliderFlags_NoRoundToFormat))
                changedAxis = axis;
            ImGui::PopStyleVar();

            if(ImGui::IsItemHovered() || ImGui::IsItemActive())
                DrawRectAroundWidget(Colors::Selection, 1.5f, 1.0f);

            ImGui::PopID();
            if(axis < 2)
                ImGui::SameLine(0.0f, spacing);
        }

        if(changedAxis >= 0)
        {
            modified = true;
            if(locked && wasReset)
                values = glm::vec3(resetValue);
            else if(locked)
            {
                // Keep the proportions: every axis scales by the edited axis' factor
                const float previous = before[changedAxis];
                if(glm::abs(previous) > 1e-6f)
                    values = before * (values[changedAxis] / previous);
                else
                    values = glm::vec3(values[changedAxis]);
            }
        }

        ImGui::PopID();
        return modified;
    }

    bool EditorUI::TDragFloat(const char* label, float* value, float speed, float min, float max, const char* format)
    {
        ValueScope scope(label);
        return ImGui::DragFloat("##v", value, speed, min, max, format, (min != 0.0f || max != 0.0f) ? ImGuiSliderFlags_AlwaysClamp : 0);
    }

    bool EditorUI::TSliderFloat(const char* label, float* value, float min, float max, const char* format)
    {
        ValueScope scope(label);
        return ImGui::SliderFloat("##v", value, min, max, format, ImGuiSliderFlags_AlwaysClamp);
    }

    bool EditorUI::TDragInt(const char* label, int* value, float speed, int min, int max)
    {
        ValueScope scope(label);
        return ImGui::DragInt("##v", value, speed, min, max, "%d", (min != 0 || max != 0) ? ImGuiSliderFlags_AlwaysClamp : 0);
    }

    bool EditorUI::TSliderInt(const char* label, int* value, int min, int max)
    {
        ValueScope scope(label);
        return ImGui::SliderInt("##v", value, min, max, "%d", ImGuiSliderFlags_AlwaysClamp);
    }

    bool EditorUI::TDragFloatN(const char* label, float* values, int components, float speed, float min, float max, const char* format)
    {
        ValueScope scope(label);
        return ImGui::DragScalarN("##v", ImGuiDataType_Float, values, components, speed, &min, &max, format, (min != 0.0f || max != 0.0f) ? ImGuiSliderFlags_AlwaysClamp : 0);
    }

    bool EditorUI::TCheckbox(const char* label, bool* value)
    {
        ValueScope scope(label);
        return ImGui::Checkbox("##v", value);
    }

    bool EditorUI::TColorEdit3(const char* label, float* rgb, ImGuiColorEditFlags flags)
    {
        ValueScope scope(label);
        return ImGui::ColorEdit3("##v", rgb, flags);
    }

    bool EditorUI::TColorEdit4(const char* label, float* rgba, ImGuiColorEditFlags flags)
    {
        ValueScope scope(label);
        return ImGui::ColorEdit4("##v", rgba, flags);
    }

    bool EditorUI::TInputText(const char* label, String& value, bool multiline)
    {
        ValueScope scope(label);
        return multiline ? ImGui::InputTextMultiline("##v", &value, ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 3.5f)) : ImGui::InputText("##v", &value);
    }

    bool EditorUI::TCombo(const char* label, int* index, const char* const* items, int count)
    {
        ValueScope scope(label);
        return ImGui::Combo("##v", index, items, count);
    }

    void EditorUI::DockSpace(float titleBarHeight, const std::function<void(ImGuiID, ImVec2)>& buildLayout, bool rebuildLayout, float bottomBarHeight)
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();

        ImVec2 dockspacePos = ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + titleBarHeight);
        ImVec2 dockspaceSize = ImVec2(viewport->WorkSize.x, viewport->WorkSize.y - titleBarHeight - bottomBarHeight);

        ImGui::SetNextWindowPos(dockspacePos);
        ImGui::SetNextWindowSize(dockspaceSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoSavedSettings;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGui::Begin("SurgeMainDockSpace", nullptr, windowFlags);
        ImGui::PopStyleVar(3);

        ImGuiID dockspaceID = ImGui::GetID("SurgeEditorDockspace");
        if(buildLayout && (rebuildLayout || !ImGui::DockBuilderGetNode(dockspaceID)))
            buildLayout(dockspaceID, dockspaceSize);
        ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

        ImGui::End();
    }

    void EditorUI::ShowConfirmationBox(const String& title, const String& message, std::function<void()> onConfirm, std::function<void()> onCancel)
    {
        sConfirmState.Title = title;
        sConfirmState.Message = message;
        sConfirmState.OnConfirm = onConfirm;
        sConfirmState.OnCancel = onCancel;
        sConfirmState.IsOpen = true;
    }

    void EditorUI::RenderConfirmationBox()
    {
        if(sConfirmState.IsOpen)
        {
            ImGui::OpenPopup(sConfirmState.Title.c_str());
            sConfirmState.IsOpen = false;
        }

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_Appearing);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 16.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, Colors::Tone12);

        bool popupOpen = true;
        ImFont* boldFont = BoldFont();

        ImGui::PushFont(boldFont);
        ImGui::PushStyleColor(ImGuiCol_Text, Colors::Gold);
        if(ImGui::BeginPopupModal(sConfirmState.Title.c_str(), &popupOpen, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::PopFont();
            ImGui::PopStyleColor();

            ImGui::TextWrapped("%s", sConfirmState.Message.c_str());
            ImGui::Dummy(ImVec2(0, 20));

            // Split the width in half + spacing
            const float buttonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

            ImGui::PushFont(boldFont);
            if(ImGui::Button("CANCEL", ImVec2(buttonWidth, 30)))
            {
                if(sConfirmState.OnCancel)
                    sConfirmState.OnCancel();
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Button, Colors::Tone15);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Colors::Accent);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, Colors::Tone10);
            if(ImGui::Button("CONFIRM", ImVec2(buttonWidth, 30)))
            {
                if(sConfirmState.OnConfirm)
                    sConfirmState.OnConfirm();
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(3);
            ImGui::PopFont();

            ImGui::EndPopup();
        }
        else
        {
            ImGui::PopFont();
            ImGui::PopStyleColor();
        }

        // User clicked the "X"
        if(!popupOpen && sConfirmState.OnCancel)
            sConfirmState.OnCancel();

        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }

} // namespace Surge
