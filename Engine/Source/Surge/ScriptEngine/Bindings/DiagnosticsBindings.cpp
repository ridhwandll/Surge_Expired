// Copyright (c) - SurgeTechnologies - All rights reserved
#include "DiagnosticsBindings.hpp"
#include "Surge/ScriptEngine/Lua.hpp"
#include "Surge/ScriptEngine/Builtin/PerformanceOverlay.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Core/Diagnostics/Diagnostics.hpp"

namespace Surge::ScriptBinding
{
    // Fills and returns the caller's table (a new one without), so a HUD calling it every frame creates no garbage
    static sol::table GetStats(sol::optional<sol::table> target, sol::this_state s)
    {
        sol::state_view lua(s);
        sol::table stats = target ? *target : lua.create_table();

        Diagnostics* diagnostics = Core::GetDiagnostics();
        const FrameTimings& timings = diagnostics->GetTimings();
        const FrameCounters& counters = diagnostics->GetCounters();

        const float frameCap = Core::GetFrameCap();
        const float targetMilliseconds = 1000.0f / (frameCap > 0.0f ? frameCap : 60.0f); // Uncapped: judged against 60 Hz

        stats.raw_set("FPS", timings.FPS, "FrameMs", timings.FrameMs, "FrameMaxMs", timings.FrameMaxMs, "CPUWorkMs", timings.CPUWorkMs,
                      "CPUWorkMaxMs", timings.CPUWorkMaxMs, "GPUWorkMs", timings.GPUWorkMs, "GPUWorkMaxMs", timings.GPUWorkMaxMs,
                      "GPUCommandBufferMs", timings.GPUCommandBufferMs, "GPUDisplayWaitMs", timings.GPUDisplayWaitMs, "LastFrameMs", timings.LastFrameMs, "LastGPUWorkMs", timings.LastGPUWorkMs,
                      "GPUTiming", timings.GPUTiming, "TargetMs", targetMilliseconds, "Thermal", Diagnostics::ThermalStatusToString(timings.ThermalStatus),
                      "CPUUsage", timings.CPUUsage);
        stats.raw_set("DrawCalls", counters.DrawCalls, "Triangles", counters.Triangles, "Meshes", counters.Meshes, "Lights", counters.Lights, "MaxLights", counters.MaxLights,
                      "Sprites", counters.Sprites, "Texts", counters.Texts, "UIItems", counters.UIItems, "Particles", counters.Particles,
                      "MaxParticles", counters.MaxParticles, "Animators", counters.Animators, "Entities", counters.Entities);
        stats.raw_set("GPUMemoryMB", counters.GPUMemoryMB, "GPUMemoryBudgetMB", counters.GPUMemoryBudgetMB, "GPUName", counters.GPUName,
                      "CPUName", counters.CPUName, "CPUThreads", counters.CPUThreads,
                      "ScreenWidth", counters.ScreenWidth, "ScreenHeight", counters.ScreenHeight, "RenderWidth", counters.RenderWidth,
                      "RenderHeight", counters.RenderHeight, "RenderScale", counters.RenderScale);

        // Groups[i] = { Name, Ms }, slowest first. The entry tables are reused, the ones past GroupCount are dropped
        sol::optional<sol::table> existingGroups = stats.raw_get<sol::optional<sol::table>>("Groups");
        sol::table groups = existingGroups ? *existingGroups : lua.create_table(static_cast<int>(timings.Groups.size()), 0);
        for(size_t i = 0; i < timings.Groups.size(); i++)
        {
            const int index = static_cast<int>(i) + 1;
            sol::optional<sol::table> existingEntry = groups.raw_get<sol::optional<sol::table>>(index);
            sol::table entry = existingEntry ? *existingEntry : lua.create_table(0, 2);
            entry.raw_set("Name", timings.Groups[i].Name, "Ms", timings.Groups[i].AverageMs);
            if(!existingEntry)
                groups.raw_set(index, entry);
        }
        for(int index = static_cast<int>(timings.Groups.size()) + 1; groups.raw_get<sol::object>(index).get_type() != sol::type::lua_nil; index++)
            groups.raw_set(index, sol::lua_nil);

        stats.raw_set("Groups", groups, "GroupCount", static_cast<Uint>(timings.Groups.size()));
        return stats;
    }

    void BindDiagnostics(void* luaState)
    {
        sol::state_view& lua = *static_cast<sol::state_view*>(luaState);
        sol::table diagnostics = lua.create_named_table("Diagnostics");

        diagnostics.set_function("GetStats", &GetStats);
        diagnostics.set_function("SetFramePacing", [](bool enabled) { Core::SetFramePacing(enabled); });
        diagnostics.set_function("IsFramePacingEnabled", []() { return Core::IsFramePacingEnabled(); });

        diagnostics.set_function("ToggleOverlay", []() { PerformanceOverlay::Toggle(); });
        diagnostics.set_function("SetOverlay", [](const String& modeName) {
            PerformanceOverlayMode mode = PerformanceOverlayMode::OFF;
            if(PerformanceOverlay::ModeFromString(modeName, mode))
                PerformanceOverlay::SetMode(mode);
            else
                Log<Severity::Warn>("[DiagnosticsBindings] Diagnostics.SetOverlay: unknown mode '{}' (Off, Compact or Detailed)", modeName);
        });
        diagnostics.set_function("GetOverlay", []() { return PerformanceOverlay::ModeToString(PerformanceOverlay::GetMode()); });
        diagnostics.set_function("SetHotkeyEnabled", [](bool enabled) { PerformanceOverlay::SetHotkeyEnabled(enabled); });
        diagnostics.set_function("IsHotkeyEnabled", []() { return PerformanceOverlay::IsHotkeyEnabled(); });
    }

} // namespace Surge::ScriptBinding
