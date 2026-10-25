// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/String.hpp"

namespace Surge
{
    class Event;

    enum class PerformanceOverlayMode : uint8_t
    {
        OFF,
        COMPACT,  // One strip: FPS, CPU, GPU
        DETAILED  // + frame time graph, GPU groups, counters
    };

    /*
    * Ctrl+F3 (or a two finger triple tap on a touch screen) cycles Off -> Compact -> Detailed -> Off in every app.
    * The overlay is an engine Lua script (PerformanceOverlay.cpp) building UI widgets into UI::Manager's debug root from
    * Diagnostics.GetStats, Lua games drive it through the Diagnostics table (DiagnosticsBindings.cpp). It only displays:
    * the numbers come from Core's Diagnostics. Hidden it costs nothing: no Lua call, no widgets drawn, nothing measured
    */
    namespace PerformanceOverlay
    {
        void Initialize(void* luaState); // ScriptEngine::Initialize, after the bindings
        void Shutdown();                 // ScriptEngine::Shutdown, before the Lua state closes
        void OnEvent(Event& e);          // Core::OnEvent: the hotkey
        void Update();                   // Core::Run, after Client::OnUpdate

        void SetMode(PerformanceOverlayMode mode);
        PerformanceOverlayMode GetMode();
        void Toggle(); // Off -> Compact -> Detailed -> Off

        // The hotkey and the touch gesture, a shipped game may lock them (Diagnostics.SetHotkeyEnabled(false))
        void SetHotkeyEnabled(bool enabled);
        bool IsHotkeyEnabled();

        const char* ModeToString(PerformanceOverlayMode mode);
        bool ModeFromString(const String& name, PerformanceOverlayMode& outMode);
    }

} // namespace Surge
