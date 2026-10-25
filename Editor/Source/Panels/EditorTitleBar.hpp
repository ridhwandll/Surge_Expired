// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/String.hpp"
#include "Surge/Graphics/RHI/RHIHandle.hpp"
#include <imgui.h>

namespace Surge
{
    class Editor;

    // The editor's own window title bar (the OS one is hidden, WindowFlags::NO_TITLEBAR), one row:
    // logo, menus and the project/scene breadcrumb on the left, play controls centered, frame stats, console counters and the window buttons on the right.
    // Empty space drags the window (Window::SetCaptionHeight). Not an IPanel: it can't be hidden from the View menu and renders before the dockspace
    class EditorTitleBar
    {
    public:
        static constexpr float HEIGHT = 40.0f;

        void Init(Editor* editor);
        void Shutdown();
        void Render(); // Project Browser variant (logo + window buttons) while no project is open

    private:
        // x values are relative to the bar. The right aligned parts return their left edge and draw nothing when they would cross minX
        void UpdatePlayModeAnimation();
        void DrawPlayModeBackground(const ImVec2& origin); // origin: the logo's center, where the amber comes from
        void DrawLogo(const ImVec2& center);
        float DrawMenus(float x); // Returns the right edge
        void DrawBreadcrumb(float x, float maxX);
        void DrawPlaybackControls(float x);
        float DrawFrameStats(float rightX, float minX);
        float DrawConsoleCounters(float rightX, float minX);
        float DrawWindowButtons(); // Flush to the top right corner

        void DrawFileMenu();
        void DrawEditMenu();
        void DrawViewMenu();
        void DrawHelpMenu();

    private:
        Editor* mEditor = nullptr;
        ImageHandle mEngineLogo;
        ImageHandle mEngineLogoWatermark; // Dark logo for the amber play mode bar
        ImVec2 mWindowPos; // Screen position of the bar, valid during Render()
        float mWidth = 0.0f;

        // Play mode transition, see UpdatePlayModeAnimation
        float mPlayBlend = 0.0f;  // 0 = edit mode .. 1 = play mode, linear in time
        float mPauseBlend = 0.0f; // 0 = running .. 1 = paused
        float mPlayTime = 0.0f;   // Unpaused seconds since the reveal finished (negative during it), drives the sweep
        bool mWasPlaying = false;

        float mStatsTimer = 0.0f;
        String mFrameTimeText;
        String mFpsText;
    };

} // namespace Surge
