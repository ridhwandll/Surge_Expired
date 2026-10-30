// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Panels/EditorTitleBar.hpp"
#include "Editor.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Core/Diagnostics/Diagnostics.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Utility/Platform.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Utility/EditorUI.hpp"
#include "Gizmos/GizmoIcons.hpp"
#include "Console/EditorConsole.hpp"
#include "Panels/ConsolePanel.hpp"
#include "Panels/ContentBrowserPanel.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <mutex>
#include <numbers>

namespace Surge
{
    namespace
    {
        namespace Colors = EditorUI::Colors;
        using EditorUI::Mix;
        using EditorUI::WithAlpha;

        constexpr float SIDE_PADDING = 4.0f;
        constexpr float LOGO_SIZE = 44.0f;
        constexpr float ITEM_HEIGHT = 28.0f; // Menu buttons, console counters
        constexpr float ITEM_ROUNDING = 2.0f;
        constexpr float MENU_PADDING_X = 10.0f;
        constexpr float GROUP_GAP = 16.0f; // Between the left, center and right groups
        constexpr float WINDOW_BUTTON_WIDTH = 40.0f;

        constexpr float FONT_SIZE = 17.0f;
        constexpr float SMALL_FONT_SIZE = 16.0f;

        // Play controls: three buttons in one pill
        constexpr ImVec2 PLAY_BUTTON_SIZE = ImVec2(32.0f, 26.0f);
        constexpr float PLAY_GROUP_PADDING = 3.0f;
        constexpr float PLAY_BUTTON_SPACING = 2.0f;
        constexpr float PLAY_GROUP_WIDTH = PLAY_BUTTON_SIZE.x * 3.0f + PLAY_BUTTON_SPACING * 2.0f + PLAY_GROUP_PADDING * 2.0f;

        // Play mode
        constexpr float PAUSED_BRIGHTNESS = 0.8f;
        constexpr float REVEAL_DURATION = 0.55f;
        constexpr float CONCEAL_DURATION = 0.4f;
        constexpr float PAUSE_FADE_DURATION = 0.1f;
        constexpr float LOGO_POP = 0.6; // Extra logo scale halfway through the transition
        constexpr float SWEEP_WIDTH = 167.0f;
        constexpr float SWEEP_DURATION = 0.6f; // Seconds to cross the bar
        constexpr float SWEEP_PERIOD = 1.0f;   // One sweep every
        constexpr ImVec4 SWEEP_COLOR = Colors::AccentPale;
        constexpr ImVec4 PLAY_TEXT_COLOR = Colors::Black;

        struct Menu
        {
            const char* Label;
            const char* PopupID;
        };
        constexpr Menu MENUS[] = { { "FILE", "##FileMenu" }, { "EDIT", "##EditMenu" }, { "VIEW", "##ViewMenu" }, { "HELP", "##HelpMenu" } };

        // Colors of everything drawn on the bar, picked once per frame (edit mode or the amber play mode bar)
        struct BarPalette
        {
            ImVec4 Background;
            ImVec4 Text;
            ImVec4 DimText;
            ImVec4 FaintText;
            ImVec4 Hover;
            ImVec4 Active;
            ImVec4 Divider;
            ImVec4 PillFill;
            ImVec4 PillBorder;
            ImVec4 Accent; // DEBUG tag, unsaved changes dot
            ImVec4 Warn;
            ImVec4 Error;
        };
        BarPalette sPalette;

        // Follows the theme, read every frame
        BarPalette EditPalette()
        {
            return { Colors::Background, Colors::TextBright, Colors::Tone62, Colors::Tone38, WithAlpha(Colors::Ink, 0.07f), WithAlpha(Colors::Ink, 0.11f),
                     WithAlpha(Colors::Ink, 0.1f), WithAlpha(Colors::Ink, 0.04f), WithAlpha(Colors::Ink, 0.07f), Colors::Accent, Colors::Warning, Colors::Error };
        }

        constexpr BarPalette PLAY_PALETTE = { Colors::Accent, PLAY_TEXT_COLOR, WithAlpha(Colors::Black, 0.7f), WithAlpha(Colors::Black, 0.42f), WithAlpha(Colors::Black, 0.1f),
                                              WithAlpha(Colors::Black, 0.18f), WithAlpha(Colors::Black, 0.22f), WithAlpha(Colors::Black, 0.08f), WithAlpha(Colors::Black, 0.14f),
                                              PLAY_TEXT_COLOR, PLAY_TEXT_COLOR, Colors::Crimson };

        float EaseOutCubic(float t)
        {
            const float inverse = 1.0f - t;
            return 1.0f - inverse * inverse * inverse;
        }

        ImVec4 PlayBackground(float pause)
        {
            return Mix(PLAY_PALETTE.Background, Mix(Colors::Background, Colors::Accent, PAUSED_BRIGHTNESS), pause);
        }

        // play: 0 = edit mode, 1 = play mode. pause: 0 = running, 1 = paused
        BarPalette MakePalette(float play, float pause)
        {
            const BarPalette a = EditPalette();
            const BarPalette& b = PLAY_PALETTE;
            return { Mix(a.Background, PlayBackground(pause), play), Mix(a.Text, b.Text, play), Mix(a.DimText, b.DimText, play), Mix(a.FaintText, b.FaintText, play),
                     Mix(a.Hover, b.Hover, play), Mix(a.Active, b.Active, play), Mix(a.Divider, b.Divider, play), Mix(a.PillFill, b.PillFill, play),
                     Mix(a.PillBorder, b.PillBorder, play), Mix(a.Accent, b.Accent, play), Mix(a.Warn, b.Warn, play), Mix(a.Error, b.Error, play) };
        }

        // Toggled buttons on the amber bar (Stop, Resume): black with an amber glyph
        void PushToggledColors()
        {
            ImGui::PushStyleColor(ImGuiCol_Button, WithAlpha(Colors::Black, 0.85f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, WithAlpha(Colors::Black, 0.7f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, Colors::Black);
        }

        void VerticalDivider(ImDrawList* drawList, float x, float centerY)
        {
            drawList->AddLine(ImVec2(x, centerY - 8.0f), ImVec2(x, centerY + 8.0f), ImGui::GetColorU32(sPalette.Divider));
        }
    } // namespace

    void EditorTitleBar::Init(Editor* editor)
    {
        mEditor = editor;
        mEngineLogo = editor->LoadEditorIcon("Editor/Assets/Textures/EngineLogo.png"); // Mipmapped, drawn well below its 200px source
        mEngineLogoWatermark = editor->LoadEditorIcon("Editor/Assets/Textures/EngineLogoWatermark.png");
    }

    void EditorTitleBar::Shutdown()
    {
        Core::GetRenderer()->GetRHI()->DestroyImage(mEngineLogo);
        Core::GetRenderer()->GetRHI()->DestroyImage(mEngineLogoWatermark);
    }

    void EditorTitleBar::Render()
    {
        const bool projectOpen = mEditor->GetCurrentProject().IsValid();
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, HEIGHT));
        ImGui::SetNextWindowViewport(viewport->ID);

        UpdatePlayModeAnimation();
        sPalette = MakePalette(EaseOutCubic(mPlayBlend), mPauseBlend);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, Colors::Background); // The amber is drawn by DrawPlayModeBackground
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                           ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
        if(ImGui::Begin("##SurgeTitleBar", nullptr, flags))
        {
            mWindowPos = ImGui::GetWindowPos();
            mWidth = ImGui::GetWindowWidth();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const float centerY = mWindowPos.y + HEIGHT * 0.5f;

            const ImVec2 logoCenter(mWindowPos.x + SIDE_PADDING + LOGO_SIZE * 0.5f, centerY);
            DrawPlayModeBackground(logoCenter);
            DrawLogo(logoCenter);
            float leftX = SIDE_PADDING + LOGO_SIZE + 8.0f;
            float rightX = DrawWindowButtons() - 8.0f;

            if(!projectOpen)
            {
                EditorUI::ScopedBoldFont font(FONT_SIZE);
                constexpr const char* title = "Surge Editor";
                const ImVec2 titleSize = ImGui::CalcTextSize(title);
                drawList->AddText(ImVec2(mWindowPos.x + leftX + 2.0f, std::floor(centerY - titleSize.y * 0.5f)), ImGui::GetColorU32(sPalette.Text), title);
                leftX += titleSize.x + 2.0f + 14.0f;
                VerticalDivider(drawList, mWindowPos.x + leftX, centerY);

                ImGui::PushFont(EditorUI::RegularFont(), FONT_SIZE);
                constexpr const char* subtitle = "Project Browser";
                drawList->AddText(ImVec2(mWindowPos.x + leftX + 14.0f, std::floor(centerY - ImGui::GetFontSize() * 0.5f)), ImGui::GetColorU32(sPalette.DimText), subtitle);
                ImGui::PopFont();
            }
            else
            {
                leftX = DrawMenus(leftX);

                // Centered on the window, pushed right when the menus need the room, dropped when even that doesn't fit
                const float playX = std::max(std::floor((mWidth - PLAY_GROUP_WIDTH) * 0.5f), leftX + GROUP_GAP);
                float breadcrumbMaxX = rightX - GROUP_GAP;
                float rightMinX = leftX + GROUP_GAP;
                if(playX + PLAY_GROUP_WIDTH + GROUP_GAP <= rightX)
                {
                    DrawPlaybackControls(playX);
                    breadcrumbMaxX = playX - GROUP_GAP;
                    rightMinX = playX + PLAY_GROUP_WIDTH + GROUP_GAP;
                }

                // Right side, dropped one by one as the window narrows: the console counters first in line, then the frame stats
                rightX = DrawConsoleCounters(rightX, rightMinX);
                DrawFrameStats(rightX - 10.0f, rightMinX);

                DrawBreadcrumb(leftX + 6.0f, breadcrumbMaxX);
            }
        }
        ImGui::End();

        ImGui::PopStyleVar(4);
        ImGui::PopStyleColor();
    }

    void EditorTitleBar::UpdatePlayModeAnimation()
    {
        // Clamped: entering play mode takes a long frame, the reveal must still play out instead of jumping to the end
        const float dt = std::min(ImGui::GetIO().DeltaTime, 1.0f / 30.0f);
        const bool playing = mEditor->IsPlaying();
        if(playing && !mWasPlaying)
        {
            mPlayTime = -REVEAL_DURATION; // First sweep once the amber has filled the bar
            mPauseBlend = 0.0f;
        }
        mWasPlaying = playing;

        mPlayBlend = playing ? std::min(mPlayBlend + dt / REVEAL_DURATION, 1.0f) : std::max(mPlayBlend - dt / CONCEAL_DURATION, 0.0f);
        if(!playing)
            return;

        const bool paused = mEditor->IsPaused();
        mPauseBlend = paused ? std::min(mPauseBlend + dt / PAUSE_FADE_DURATION, 1.0f) : std::max(mPauseBlend - dt / PAUSE_FADE_DURATION, 0.0f);
        if(!paused)
            mPlayTime += dt;
    }

    void EditorTitleBar::DrawPlayModeBackground(const ImVec2& origin)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 max(mWindowPos.x + mWidth, mWindowPos.y + HEIGHT);
        if(mPlayBlend < 1.0f)
            drawList->AddLine(ImVec2(mWindowPos.x, max.y - 1.0f), ImVec2(max.x, max.y - 1.0f), ImGui::GetColorU32(Colors::Border));
        if(mPlayBlend <= 0.0f)
            return;

        const ImU32 fill = ImGui::GetColorU32(PlayBackground(mPauseBlend));
        if(mPlayBlend < 1.0f)
        {
            // Entering/leaving: a disc of amber grows out of the logo (shrinks back into it) with a light rim at its edge.
            // Linear blend through an ease out: fast out of the logo then settling, slow off the far end then sucked back in
            const ImVec2 farCorner(max.x - origin.x, HEIGHT * 0.5f);
            const float radius = EaseOutCubic(mPlayBlend) * std::sqrt(farCorner.x * farCorner.x + farCorner.y * farCorner.y);
            const float rim = std::sin(mPlayBlend * std::numbers::pi_v<float>);
            drawList->AddCircleFilled(origin, radius, fill);
            drawList->AddCircle(origin, radius, ImGui::GetColorU32(WithAlpha(SWEEP_COLOR, 0.8f * rim)), 0, 3.0f);
            return;
        }
        drawList->AddRectFilled(mWindowPos, max, fill);

        // Running: a light leaves the logo and sweeps across the bar like an indeterminate progress bar, frozen and faded out while paused
        if(mPlayTime < 0.0f || mPauseBlend >= 1.0f)
            return;

        const float sweep = std::fmod(mPlayTime, SWEEP_PERIOD) / SWEEP_DURATION;
        if(sweep >= 1.0f)
            return;

        const float center = origin.x + sweep * (max.x - origin.x + SWEEP_WIDTH * 0.5f);
        const float alpha = 0.5f * std::min(sweep * 4.0f, 1.0f) * (1.0f - mPauseBlend);
        const ImU32 clear = ImGui::GetColorU32(WithAlpha(SWEEP_COLOR, 0.0f));
        const ImU32 bright = ImGui::GetColorU32(WithAlpha(SWEEP_COLOR, alpha));
        drawList->AddRectFilledMultiColor(ImVec2(center - SWEEP_WIDTH * 0.5f, mWindowPos.y), ImVec2(center, max.y), clear, bright, bright, clear);
        drawList->AddRectFilledMultiColor(ImVec2(center, mWindowPos.y), ImVec2(center + SWEEP_WIDTH * 0.5f, max.y), bright, clear, clear, bright);
    }

    void EditorTitleBar::DrawLogo(const ImVec2& center)
    {
        // Pops while the bar changes color, crossfading from the amber logo to the dark one (the amber one would vanish on the amber bar)
        const float half = LOGO_SIZE * 0.5f * (1.0f + LOGO_POP * std::sin(mPlayBlend * std::numbers::pi_v<float>));
        const ImVec2 min(std::floor(center.x - half), std::floor(center.y - half));
        const ImVec2 max(min.x + half * 2.0f, min.y + half * 2.0f);
        const float watermark = EaseOutCubic(mPlayBlend);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        GraphicsRHI* rhi = Core::GetRenderer()->GetRHI().get();
        if(watermark < 1.0f)
            drawList->AddImage(rhi->GetImGuiImage(mEngineLogo), min, max, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), ImGui::GetColorU32(WithAlpha(Colors::White, 1.0f - watermark)));
        if(watermark > 0.0f)
            drawList->AddImage(rhi->GetImGuiImage(mEngineLogoWatermark), min, max, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), ImGui::GetColorU32(WithAlpha(Colors::White, watermark)));
    }

    float EditorTitleBar::DrawMenus(float x)
    {
        bool anyOpen = false;
        for(const Menu& menu : MENUS)
            anyOpen |= ImGui::IsPopupOpen(menu.PopupID);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const float y = std::floor((HEIGHT - ITEM_HEIGHT) * 0.5f);
        float popupX[std::size(MENUS)];

        ImGui::PushFont(EditorUI::RegularFont(), FONT_SIZE);
        for(size_t i = 0; i < std::size(MENUS); i++)
        {
            const Menu& menu = MENUS[i];
            const ImVec2 textSize = ImGui::CalcTextSize(menu.Label);

            ImGui::SetCursorPos(ImVec2(x, y));
            const bool pressed = ImGui::InvisibleButton(menu.Label, ImVec2(textSize.x + MENU_PADDING_X * 2.0f, ITEM_HEIGHT));
            const bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup);
            bool open = ImGui::IsPopupOpen(menu.PopupID);

            // Like an OS menu bar: once a menu is open, hovering the others switches to them. Clicking the open one closes it (click outside the popup)
            if(!open && (pressed || (hovered && anyOpen)))
            {
                ImGui::OpenPopup(menu.PopupID);
                open = true;
            }

            const ImVec2 min = ImGui::GetItemRectMin();
            const ImVec2 max = ImGui::GetItemRectMax();
            if(open || hovered)
                drawList->AddRectFilled(min, max, ImGui::GetColorU32(open ? sPalette.Active : sPalette.Hover), ITEM_ROUNDING);
            drawList->AddText(ImVec2(min.x + MENU_PADDING_X, std::floor(min.y + (ITEM_HEIGHT - textSize.y) * 0.5f)), ImGui::GetColorU32(open || hovered ? sPalette.Text : sPalette.DimText), menu.Label);

            popupX[i] = min.x;
            x = max.x - mWindowPos.x + 2.0f;
        }
        ImGui::PopFont();

        // Each popup opens right under its button
        const float popupY = mWindowPos.y + HEIGHT + 2.0f;
        EditorUI::StyledPopupVars::Push();
        ImGui::SetNextWindowPos(ImVec2(popupX[0], popupY));
        DrawFileMenu();
        ImGui::SetNextWindowPos(ImVec2(popupX[1], popupY));
        DrawEditMenu();
        ImGui::SetNextWindowPos(ImVec2(popupX[2], popupY));
        DrawViewMenu();
        ImGui::SetNextWindowPos(ImVec2(popupX[3], popupY));
        DrawHelpMenu();
        EditorUI::StyledPopupVars::Pop();

        return x;
    }

    void EditorTitleBar::DrawBreadcrumb(float x, float maxX)
    {
        if(maxX - x < 60.0f)
            return;

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const float centerY = mWindowPos.y + HEIGHT * 0.5f;
        drawList->PushClipRect(ImVec2(mWindowPos.x + x, mWindowPos.y), ImVec2(mWindowPos.x + maxX, mWindowPos.y + HEIGHT), true);

        VerticalDivider(drawList, mWindowPos.x + x, centerY);
        float cursor = mWindowPos.x + x + 14.0f;

#ifdef SURGE_DEBUG
        {
            EditorUI::ScopedBoldFont font(SMALL_FONT_SIZE);
            const ImVec2 textSize = ImGui::CalcTextSize("DEBUG");
            const ImVec2 min(cursor, std::floor(centerY - textSize.y * 0.5f) - 3.0f);
            const ImVec2 max(min.x + textSize.x + 12.0f, min.y + textSize.y + 6.0f);
            drawList->AddText(ImVec2(min.x + 6.0f, min.y + 3.0f), ImGui::GetColorU32(sPalette.Accent), "DEBUG");
            cursor = max.x + 10.0f;
        }
#endif

        {
            EditorUI::ScopedBoldFont font(FONT_SIZE);
            const char* projectName = mEditor->GetCurrentProject().Name.c_str();
            const ImVec2 textSize = ImGui::CalcTextSize(projectName);
            drawList->AddText(ImVec2(cursor, std::floor(centerY - textSize.y * 0.5f)), ImGui::GetColorU32(sPalette.Text), projectName);
            cursor += textSize.x + 9.0f;
        }

        // Chevron between the project and the scene
        const ImVec2 chevron[] = { ImVec2(cursor, centerY - 4.0f), ImVec2(cursor + 4.0f, centerY), ImVec2(cursor, centerY + 4.0f) };
        drawList->AddPolyline(chevron, static_cast<int>(std::size(chevron)), ImGui::GetColorU32(sPalette.FaintText), ImDrawFlags_None, 1.5f);
        cursor += 4.0f + 9.0f;

        const AssetID sceneID = mEditor->GetCurrentScene()->GetID();
        AssetManager* assetManager = Core::GetAssetManager();
        const String sceneName = sceneID && assetManager->IsRegistered(sceneID) ? Filesystem::GetFilenameWithoutExt(assetManager->GetMetadata(sceneID).RelativePath) : String("Untitled");
        ImGui::PushFont(EditorUI::RegularFont(), FONT_SIZE);
        const ImVec2 sceneSize = ImGui::CalcTextSize(sceneName.c_str());
        drawList->AddText(ImVec2(cursor, std::floor(centerY - sceneSize.y * 0.5f)), ImGui::GetColorU32(sPalette.DimText), sceneName.c_str());
        ImGui::PopFont();
        cursor += sceneSize.x;

        // Unsaved changes
        if(mEditor->GetHistory().IsDirty())
            drawList->AddCircleFilled(ImVec2(cursor + 9.0f, centerY + 1.0f), 3.5f, ImGui::GetColorU32(sPalette.Accent));

        drawList->PopClipRect();
    }

    void EditorTitleBar::DrawPlaybackControls(float x)
    {
        const bool playing = mEditor->IsPlaying();
        const bool paused = mEditor->IsPaused();
        const ImVec4 amber = Colors::Accent;

        // The pill behind the buttons
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const float groupHeight = PLAY_BUTTON_SIZE.y + PLAY_GROUP_PADDING * 2.0f;
        const float groupY = std::floor((HEIGHT - groupHeight) * 0.5f);
        const ImVec2 groupMin(mWindowPos.x + x, mWindowPos.y + groupY);
        const ImVec2 groupMax(groupMin.x + PLAY_GROUP_WIDTH, groupMin.y + groupHeight);
        drawList->AddRectFilled(groupMin, groupMax, ImGui::GetColorU32(sPalette.PillFill), ITEM_ROUNDING + PLAY_GROUP_PADDING);
        drawList->AddRect(groupMin, groupMax, ImGui::GetColorU32(sPalette.PillBorder), ITEM_ROUNDING + PLAY_GROUP_PADDING);

        ImGui::PushStyleColor(ImGuiCol_Button, Colors::Transparent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, sPalette.Hover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, sPalette.Active);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, ITEM_ROUNDING);
        ImGui::SetCursorPos(ImVec2(x + PLAY_GROUP_PADDING, groupY + PLAY_GROUP_PADDING));

        // Play/Stop: shown toggled while running
        if(playing)
            PushToggledColors();
        if(GizmoIcons::IconButton("##PlayStop", playing ? GizmoIcon::STOP : GizmoIcon::PLAY, PLAY_BUTTON_SIZE, playing ? amber : sPalette.Text, 0.55f))
        {
            if(playing)
                mEditor->OnRuntimeEnd();
            else
                mEditor->OnRuntimeStart();
        }
        if(playing)
            ImGui::PopStyleColor(3);
        EditorUI::ItemTooltip(playing ? "Stop (F5)" : "Play (F5)");

        if(!playing)
            ImGui::BeginDisabled();

        ImGui::SameLine(0.0f, PLAY_BUTTON_SPACING);
        if(paused)
            PushToggledColors();
        if(GizmoIcons::IconButton("##PauseResume", paused ? GizmoIcon::PLAY : GizmoIcon::PAUSE, PLAY_BUTTON_SIZE, paused ? amber : sPalette.Text, 0.55f))
            mEditor->SetPaused(!paused);
        if(paused)
            ImGui::PopStyleColor(3);
        EditorUI::ItemTooltip(paused ? "Resume (F6)" : "Pause (F6)");

        ImGui::SameLine(0.0f, PLAY_BUTTON_SPACING);
        if(GizmoIcons::IconButton("##Step", GizmoIcon::STEP, PLAY_BUTTON_SIZE, sPalette.Text, 0.55f))
            mEditor->StepFrame();
        EditorUI::ItemTooltip("Step one frame, pauses first (F7)");

        if(!playing)
            ImGui::EndDisabled();

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
    }

    float EditorTitleBar::DrawFrameStats(float rightX, float minX)
    {
        // Read every frame: keeps Diagnostics measuring for as long as the Editor runs (averaged frame time, same numbers as the overlay)
        const FrameTimings& timings = Core::GetDiagnostics()->GetTimings();
        mStatsTimer += ImGui::GetIO().DeltaTime;
        if(mStatsTimer >= 0.25f || mFpsText.empty())
        {
            mFrameTimeText = std::format("{:.2f} ms", timings.FrameMs);
            mFpsText = std::format("{:.0f} FPS", timings.FPS);
            mStatsTimer = 0.0f;
        }

        ImGui::PushFont(EditorUI::RegularFont(), SMALL_FONT_SIZE);
        constexpr float gap = 8.0f;
        const ImVec2 frameTimeSize = ImGui::CalcTextSize(mFrameTimeText.c_str());
        const ImVec2 fpsSize = ImGui::CalcTextSize(mFpsText.c_str());
        const float width = frameTimeSize.x + gap + fpsSize.x;
        if(rightX - width < minX)
        {
            ImGui::PopFont();
            return rightX;
        }

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const float x = mWindowPos.x + rightX - width;
        const float y = std::floor(mWindowPos.y + (HEIGHT - frameTimeSize.y) * 0.5f);
        drawList->AddText(ImVec2(x, y), ImGui::GetColorU32(sPalette.DimText), mFrameTimeText.c_str());
        drawList->AddText(ImVec2(x + frameTimeSize.x + gap, y), ImGui::GetColorU32(sPalette.Text), mFpsText.c_str());
        ImGui::PopFont();
        return rightX - width;
    }

    float EditorTitleBar::DrawConsoleCounters(float rightX, float minX)
    {
        EditorConsole& console = EditorConsole::Get();
        Uint warnings = 0, errors = 0;
        {
            std::lock_guard<std::mutex> lock(console.GetMutex());
            warnings = console.GetWarningCount();
            errors = console.GetErrorCount();
        }

        char warningText[16], errorText[16];
        std::snprintf(warningText, sizeof(warningText), "%u", warnings);
        std::snprintf(errorText, sizeof(errorText), "%u", errors);
        constexpr const char* warningLabel = "WARN: ";
        constexpr const char* errorLabel = "ERR: ";

        ImGui::PushFont(EditorUI::RegularFont(), SMALL_FONT_SIZE);
        constexpr float paddingX = 8.0f;
        constexpr float counterGap = 12.0f;
        const float warningLabelWidth = ImGui::CalcTextSize(warningLabel).x;
        const float errorLabelWidth = ImGui::CalcTextSize(errorLabel).x;
        const ImVec2 warningSize = ImGui::CalcTextSize(warningText);
        const ImVec2 errorSize = ImGui::CalcTextSize(errorText);
        const float width = paddingX * 2.0f + warningLabelWidth + warningSize.x + counterGap + errorLabelWidth + errorSize.x;
        if(rightX - width < minX)
        {
            ImGui::PopFont();
            return rightX;
        }

        ImGui::SetCursorPos(ImVec2(rightX - width, std::floor((HEIGHT - ITEM_HEIGHT) * 0.5f)));
        if(ImGui::InvisibleButton("##ConsoleCounters", ImVec2(width, ITEM_HEIGHT)))
            mEditor->GetPanelManager().GetPanel<ConsolePanel>()->Focus();
        const bool hovered = ImGui::IsItemHovered();
        if(hovered)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        if(hovered)
            drawList->AddRectFilled(min, max, ImGui::GetColorU32(ImGui::IsItemActive() ? sPalette.Active : sPalette.Hover), ITEM_ROUNDING);

        // WARN: x ERR: x, each label in its severity color while its count isn't zero
        const float textY = std::floor((min.y + max.y - warningSize.y) * 0.5f);
        float x = min.x + paddingX;
        drawList->AddText(ImVec2(x, textY), ImGui::GetColorU32(warnings > 0 ? sPalette.Warn : sPalette.FaintText), warningLabel);
        x += warningLabelWidth;
        drawList->AddText(ImVec2(x, textY), ImGui::GetColorU32(warnings > 0 ? sPalette.Text : sPalette.FaintText), warningText);
        x += warningSize.x + counterGap;
        drawList->AddText(ImVec2(x, textY), ImGui::GetColorU32(errors > 0 ? sPalette.Error : sPalette.FaintText), errorLabel);
        x += errorLabelWidth;
        drawList->AddText(ImVec2(x, textY), ImGui::GetColorU32(errors > 0 ? sPalette.Text : sPalette.FaintText), errorText);
        ImGui::PopFont();

        if(hovered)
            EditorUI::ItemTooltip(std::format("{} warnings, {} errors. Click to open the Console", warnings, errors).c_str());
        return rightX - width;
    }

    float EditorTitleBar::DrawWindowButtons()
    {
        enum class WindowButton
        {
            MINIMIZE,
            MAXIMIZE,
            CLOSE
        };
        constexpr WindowButton buttons[] = { WindowButton::MINIMIZE, WindowButton::MAXIMIZE, WindowButton::CLOSE };
        constexpr const char* ids[] = { "##Minimize", "##Maximize", "##Close" };

        Window* window = Core::GetWindow();
        const bool maximized = window->IsWindowMaximized();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const float startX = mWidth - WINDOW_BUTTON_WIDTH * static_cast<float>(std::size(buttons));

        for(size_t i = 0; i < std::size(buttons); i++)
        {
            const WindowButton button = buttons[i];
            const bool isClose = button == WindowButton::CLOSE;

            ImGui::SetCursorPos(ImVec2(startX + WINDOW_BUTTON_WIDTH * static_cast<float>(i), 0.0f));
            const bool pressed = ImGui::InvisibleButton(ids[i], ImVec2(WINDOW_BUTTON_WIDTH, HEIGHT));
            const bool hovered = ImGui::IsItemHovered();
            const bool held = ImGui::IsItemActive();
            const ImVec2 min = ImGui::GetItemRectMin();
            const ImVec2 max = ImGui::GetItemRectMax();

            if(hovered || held)
            {
                ImVec4 background = isClose ? Colors::CloseHover : (held ? sPalette.Active : sPalette.Hover);
                if(isClose && held)
                    background.w = 0.8f;
                drawList->AddRectFilled(min, max, ImGui::GetColorU32(background));
            }

            // Thin Windows 11 style glyphs on whole pixels, ImDrawList adds the half pixel offset itself
            const ImU32 color = ImGui::GetColorU32((isClose && hovered) ? Colors::White : sPalette.Text);
            const ImVec2 c(std::floor((min.x + max.x) * 0.5f), std::floor((min.y + max.y) * 0.5f));
            switch(button)
            {
                case WindowButton::MINIMIZE:
                    drawList->AddLine(ImVec2(c.x - 5.0f, c.y), ImVec2(c.x + 5.0f, c.y), color);
                    break;
                case WindowButton::MAXIMIZE:
                    if(maximized) // Restore: two overlapping squares
                    {
                        drawList->AddRect(ImVec2(c.x - 5.0f, c.y - 3.0f), ImVec2(c.x + 4.0f, c.y + 6.0f), color, 1.0f);
                        const ImVec2 back[] = { ImVec2(c.x - 3.0f, c.y - 3.0f), ImVec2(c.x - 3.0f, c.y - 5.0f), ImVec2(c.x + 5.0f, c.y - 5.0f), ImVec2(c.x + 5.0f, c.y + 3.0f), ImVec2(c.x + 3.0f, c.y + 3.0f) };
                        drawList->AddPolyline(back, static_cast<int>(std::size(back)), color, ImDrawFlags_None, 1.0f);
                    }
                    else
                        drawList->AddRect(ImVec2(c.x - 5.0f, c.y - 5.0f), ImVec2(c.x + 6.0f, c.y + 6.0f), color, 1.0f);
                    break;
                case WindowButton::CLOSE:
                {
                    // Diagonals have no pixel grid to snap to: center them on the button, not on the pixel (-0.5 cancels AddLine's offset)
                    const ImVec2 d(c.x - 0.5f, c.y - 0.5f);
                    drawList->AddLine(ImVec2(d.x - 5.0f, d.y - 5.0f), ImVec2(d.x + 5.0f, d.y + 5.0f), color);
                    drawList->AddLine(ImVec2(d.x + 5.0f, d.y - 5.0f), ImVec2(d.x - 5.0f, d.y + 5.0f), color);
                    break;
                }
            }

            if(!pressed)
                continue;

            switch(button)
            {
                case WindowButton::MINIMIZE:
                    window->Minimize();
                    break;
                case WindowButton::MAXIMIZE:
                    if(maximized)
                        window->RestoreFromMaximize();
                    else
                        window->Maximize();
                    break;
                case WindowButton::CLOSE:
                    mEditor->ConfirmDiscardChanges("Exit", []() { Platform::RequestExit(); });
                    break;
            }
        }
        return startX;
    }

    void EditorTitleBar::DrawFileMenu()
    {
        if(!ImGui::BeginPopup("##FileMenu"))
            return;

        EditorUI::MenuHeader("FILE");

        const bool playing = mEditor->IsPlaying();
        AssetManager* assetManager = Core::GetAssetManager();
        const Project& project = mEditor->GetCurrentProject();
        const AssetID currentSceneID = mEditor->GetCurrentScene()->GetID();
        const bool sceneIsAsset = currentSceneID && assetManager->IsRegistered(currentSceneID);

        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, Colors::Tone18);
        if(EditorUI::StyledMenuItem("New Scene", "Ctrl+N", nullptr, !playing))
            mEditor->NewScene();

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));
        if(ImGui::BeginMenu("Open Scene", !playing))
        {
            // Sorted by path, the registry map has no stable order
            Vector<Pair<String, AssetID>> scenes;
            for(const auto& [id, metadata] : assetManager->GetRegistryMap())
            {
                if(metadata.Type == AssetType::SCENE && !HasFlag(metadata.Flags, AssetFlags::MISSING))
                    scenes.push_back({ metadata.RelativePath, id });
            }
            std::sort(scenes.begin(), scenes.end(), [](const auto& a, const auto& b) { return a.Data1 < b.Data1; });

            if(scenes.empty())
                ImGui::TextDisabled("No scenes in the project");
            for(const auto& [path, id] : scenes)
            {
                const bool isCurrent = id == currentSceneID;
                const char* tag = isCurrent ? "OPEN" : (id == project.StartScene ? "START" : nullptr);
                if(EditorUI::StyledMenuItem(path.c_str(), tag, nullptr, !isCurrent))
                    mEditor->OpenScene(id);
            }
            ImGui::EndMenu();
        }
        ImGui::PopStyleVar();

        EditorUI::StyledSeparator();

        if(EditorUI::StyledMenuItem("Save Scene", "Ctrl+S", nullptr, sceneIsAsset))
            mEditor->SaveScene();
        if(EditorUI::StyledMenuItem("Save Scene As...", "Ctrl+Shift+S", nullptr, !playing))
            mEditor->OpenSaveSceneAsPopup();
        if(EditorUI::StyledMenuItem("Set as Start Scene", nullptr, nullptr, sceneIsAsset && currentSceneID != project.StartScene && !mEditor->GetProjectFilePath().empty()))
            mEditor->SetStartScene(currentSceneID);

        EditorUI::StyledSeparator();

        if(EditorUI::StyledMenuItem("Show Project in Explorer"))
            Platform::OpenFolderInExplorer(Filesystem::GetParentPath(mEditor->GetProjectFilePath()).string());
        if(EditorUI::StyledMenuItem("Close Project"))
            mEditor->CloseProject();
        if(EditorUI::StyledMenuItem("Exit"))
            mEditor->ConfirmDiscardChanges("Exit", []() { Platform::RequestExit(); });
        ImGui::PopStyleColor();

        ImGui::EndPopup();
    }

    void EditorTitleBar::DrawEditMenu()
    {
        if(!ImGui::BeginPopup("##EditMenu"))
            return;

        EditorUI::MenuHeader("EDIT");

        const bool playing = mEditor->IsPlaying();
        EditorHistory& history = mEditor->GetHistory();
        const String undoLabel = history.CanUndo() ? std::format("Undo {}", history.GetUndoName()) : String("Undo");
        const String redoLabel = history.CanRedo() ? std::format("Redo {}", history.GetRedoName()) : String("Redo");

        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, Colors::Tone18);
        if(EditorUI::StyledMenuItem(undoLabel.c_str(), "Ctrl+Z", nullptr, !playing && history.CanUndo()))
            mEditor->RequestUndo();
        if(EditorUI::StyledMenuItem(redoLabel.c_str(), "Ctrl+Y", nullptr, !playing && history.CanRedo()))
            mEditor->RequestRedo();

        EditorUI::StyledSeparator();

        Entity selected = mEditor->GetSelectedEntity();
        const bool hasSelection = (bool)selected;
        if(EditorUI::StyledMenuItem("Copy", "Ctrl+C", nullptr, hasSelection))
            mEditor->CopyEntity(selected);
        if(EditorUI::StyledMenuItem("Paste", "Ctrl+V", nullptr, !playing && mEditor->HasEntityInClipboard()))
            mEditor->PasteEntity();
        if(EditorUI::StyledMenuItem("Duplicate", "Ctrl+D", nullptr, hasSelection))
            mEditor->DuplicateEntity(selected);
        if(EditorUI::StyledMenuItem("Delete", "Del", nullptr, hasSelection))
            mEditor->DeleteEntity(selected);

        EditorUI::StyledSeparator();

        if(EditorUI::StyledMenuItem("Focus Selection", "F", nullptr, hasSelection))
            mEditor->FocusEntity(selected);
        ImGui::PopStyleColor();

        ImGui::EndPopup();
    }

    void EditorTitleBar::DrawViewMenu()
    {
        if(!ImGui::BeginPopup("##ViewMenu"))
            return;

        EditorUI::MenuHeader("EDITOR PANELS");

        PanelManager& panels = mEditor->GetPanelManager();
        ViewportManager& viewports = mEditor->GetViewports();
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, Colors::Tone18);
        for(auto& [panelCode, panel] : panels.GetAllPanels())
            ImGui::MenuItem(PanelCodeToString(panelCode), nullptr, &panel.Show);
        if(EditorUI::StyledMenuItem("Content Drawer", "Ctrl+Space"))
            panels.GetPanel<ContentBrowserPanel>()->ToggleDrawer();

        EditorUI::StyledSeparator();
        {
            EditorUI::ScopedBoldFont font;
            ImGui::TextColored(Colors::Hint, "VIEWPORTS");
        }
        viewports.DrawMenuItems();
        EditorUI::StyledSeparator();

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));
        if(ImGui::BeginMenu("Theme"))
        {
            bool dark = !mEditor->IsLightTheme();
            bool light = !dark;
            if(EditorUI::StyledMenuItem("Normal", nullptr, &dark))
                mEditor->SetLightTheme(false);
            if(EditorUI::StyledMenuItem("Flashbang", nullptr, &light))
                mEditor->SetLightTheme(true);
            ImGui::EndMenu();
        }
        ImGui::PopStyleVar();

        if(ImGui::MenuItem("Reset Layout"))
        {
            for(auto& [panelCode, panel] : panels.GetAllPanels())
                panel.Show = true;
            if(!viewports.GetActiveSceneView())
                viewports.AddSceneView();
            if(!viewports.GetGameView())
                viewports.OpenGameView();
            mEditor->RequestResetLayout(); // Rebuilt right before the dockspace next frame
        }
        ImGui::PopStyleColor();

        ImGui::EndPopup();
    }

    void EditorTitleBar::DrawHelpMenu()
    {
        if(!ImGui::BeginPopup("##HelpMenu"))
            return;

        EditorUI::MenuHeader("HELP");

        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, Colors::Tone18);
        if(EditorUI::StyledMenuItem("Keyboard Shortcuts"))
            mEditor->OpenShortcutsWindow();
        ImGui::PopStyleColor();

        ImGui::EndPopup();
    }

} // namespace Surge
