// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/ScriptEngine/Builtin/PerformanceOverlay.hpp"
#include "Surge/ScriptEngine/Lua.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Core/Diagnostics/Diagnostics.hpp"
#include "Surge/Core/Profiler.hpp"
#include "Surge/Core/Time/Clock.hpp"
#include "Surge/Core/Events/Event.hpp"
#include "Surge/Core/Input/Input.hpp"
#include "Surge/Core/Window/Window.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"

#include <algorithm>
#include <cctype>

namespace Surge
{
    // The overlay: plain Lua on the engine's UI bindings, in its own environment (reads fall back to _G, nothing is added to it).
    // Called with setRoot(widget|nil), returns { SetMode(name), Update(dt) }. Allocation free per frame: the graph moves a write head
    // (2 bar writes a frame), colors are shared Vec4s (setters copy), text is rebuilt 4 times a second.
    // Greys only: the one color marks a number over its budget
    static constexpr const char* OVERLAY_SCRIPT = R"lua(
local setRoot = ...
local Vec2, Vec4 = Math.Vec2.new, Math.Vec4.new
local format, floor, min, max = string.format, math.floor, math.min, math.max

local TEXT     = Vec4(0.88, 0.88, 0.88, 1.0)
local DIM      = Vec4(0.52, 0.52, 0.52, 1.0)
local OVER     = Vec4(0.92, 0.47, 0.43, 1.0)
local AMBER    = Vec4(1.0, 0.655, 0.0, 1.0) -- The engine's amber (title bar, menu glyph)
local PANEL    = Vec4(0.05, 0.05, 0.05, 0.78)
local TRACK    = Vec4(1.0, 1.0, 1.0, 0.06)
local BAR      = Vec4(1.0, 1.0, 1.0, 0.32)
local BAR_OVER = Vec4(0.92, 0.47, 0.43, 0.8)
local LINE     = Vec4(1.0, 1.0, 1.0, 0.28)

local WIDTH, STRIP, MARGIN, PAD, GAP = 300, 24, 8, 10, 3
local INNER = WIDTH - PAD * 2
local GRAPH_BARS, GRAPH_HEIGHT = 60, 40
local GROUP_ROWS, ROW_HEIGHT = 5, 16
local COUNTER_HEIGHT = 98 -- Seven lines at 11
local TAG = 30 -- CPU/GPU tag column in front of the device names
local COLUMN = (INNER - 16) / 2 -- Counters: two label/value columns with a gutter
local TEXT_INTERVAL = 0.25

local stats = {}
local ui = nil
local mode = "Off"
local textTimer = TEXT_INTERVAL
local graphHead = 1
local scratch = Vec2(0, 0) -- Reused for runtime Size writes

-- Frame time includes the frame cap: on target is fine, only a miss is marked
local function FrameColor(ms, target) return ms <= target * 1.05 and TEXT or OVER end
local function BudgetColor(ms, target) return ms <= target and TEXT or OVER end

local function Count(n)
    if n >= 1000000 then return format("%.1fM", n / 1000000) end
    if n >= 10000 then return format("%.0fk", n / 1000) end
    if n >= 1000 then return format("%.1fk", n / 1000) end
    return tostring(n)
end

local function Place(widget, parent, x, y, w, h)
    widget.Anchor = Vec2(0, 0)
    widget.Pivot = Vec2(0, 0)
    widget.Offset = Vec2(x, y)
    widget.Size = Vec2(w, h)
    parent:AddChild(widget)
    return widget
end

local function Box(parent, x, y, w, h, color, radius)
    local image = Place(UIImage.new(""), parent, x, y, w, h)
    image.Color = color
    image.CornerRadius = radius or 0
    return image
end

local function Label(parent, x, y, w, h, size, color, alignment, vertical)
    local text = Place(UIText.new(""), parent, x, y, w, h)
    text.FontSize = size
    text.Color = color
    text.TextAlignment = alignment or TextAlignment.LEFT
    text.TextVAlignment = vertical or TextVerticalAlignment.CENTER
    text.Overflow = UITextOverflow.NO_WRAP
    return text
end

local function Build()
    ui = { bars = {}, rows = {} }
    ui.root = UIWidget.new()
    local safe = UISafeArea.new()
    safe.AnchorMin = Vec2(0, 0)
    safe.AnchorMax = Vec2(1, 1)
    safe.Size = Vec2(0, 0) -- Stretched by the anchors, a widget starts at 100 x 100
    ui.root:AddChild(safe)

    -- Compact strip, also the header of the detailed view: FPS | CPU | GPU in three equal columns
    local strip = Box(safe, MARGIN, MARGIN, WIDTH, STRIP, PANEL, 4)
    local third = INNER / 3
    ui.fps = Label(strip, PAD, 0, third, STRIP, 13, TEXT)
    ui.cpu = Label(strip, PAD + third, 0, third, STRIP, 13, TEXT, TextAlignment.CENTER)
    ui.gpu = Label(strip, PAD + third * 2, 0, third, STRIP, 13, TEXT, TextAlignment.RIGHT)

    local y = 8
    local detail = Box(safe, MARGIN, MARGIN + STRIP + GAP, WIDTH, 0, PANEL, 4) -- Height set once the content is laid out
    ui.detail = detail

    -- Header: the engine and the averaged frame time, then the CPU and GPU on a line each: the full names fit (~40 characters)
    Label(detail, PAD, y, INNER - 130, 14, 11, AMBER).Text = "SurgeEngine // RidT"
    ui.frame = Label(detail, PAD + INNER - 130, y, 130, 14, 11, TEXT, TextAlignment.RIGHT)
    y = y + 15
    Label(detail, PAD, y, TAG, 14, 11, DIM).Text = "CPU"
    ui.cpuName = Label(detail, PAD + TAG, y, INNER - TAG, 14, 11, TEXT)
    y = y + 15
    Label(detail, PAD, y, TAG, 14, 11, DIM).Text = "GPU"
    ui.gpuName = Label(detail, PAD + TAG, y, INNER - TAG, 14, 11, TEXT)
    y = y + 20

    -- Frame time graph, 2x the target high: the line marks the target. Its numbers sit above it, never over the bars
    Label(detail, PAD, y, INNER / 2, 14, 11, DIM).Text = "FRAME TIME"
    ui.budget = Label(detail, PAD + INNER / 2, y, INNER / 2, 14, 11, DIM, TextAlignment.RIGHT)
    y = y + 16
    local graph = Box(detail, PAD, y, INNER, GRAPH_HEIGHT, TRACK, 2)
    local barStep = INNER / GRAPH_BARS
    for i = 1, GRAPH_BARS do
        local bar = Box(graph, (i - 1) * barStep, 0, barStep - 1, GRAPH_HEIGHT, BAR)
        bar.ImageMode = UIImageMode.FILLED
        bar.FillOrigin = UIFillOrigin.BOTTOM
        bar.FillAmount = 0
        ui.bars[i] = bar
    end
    Box(graph, 0, GRAPH_HEIGHT * 0.5 - 0.5, INNER, 1, LINE)
    y = y + GRAPH_HEIGHT + 10

    -- GPU groups, slowest first: title (+ the display wait, idle not work), then name | ms | share
    ui.gpuTitle = Label(detail, PAD, y, INNER, 14, 11, DIM)
    y = y + 16
    for r = 1, GROUP_ROWS do
        ui.rows[r] = {
            name = Label(detail, PAD, y, INNER - 110, ROW_HEIGHT, 12, TEXT),
            ms = Label(detail, PAD + INNER - 110, y, 70, ROW_HEIGHT, 12, TEXT, TextAlignment.RIGHT),
            share = Label(detail, PAD + INNER - 40, y, 40, ROW_HEIGHT, 12, DIM, TextAlignment.RIGHT),
        }
        y = y + ROW_HEIGHT
    end
    y = y + 6
    Box(detail, PAD, y, INNER, 1, TRACK)
    y = y + 8

    -- Counters: two columns, each a label text (left) and a value text (right aligned) of six lines
    local top = TextVerticalAlignment.TOP
    local right = PAD + COLUMN + 16
    Label(detail, PAD, y, COLUMN, COUNTER_HEIGHT, 11, DIM, TextAlignment.LEFT, top).Text = "Draws / tris\nMeshes\nLights\nParticles\nUI items\nAnimators\nCPU threads"
    ui.left = Label(detail, PAD, y, COLUMN, COUNTER_HEIGHT, 11, TEXT, TextAlignment.RIGHT, top)
    Label(detail, right, y, COLUMN, COUNTER_HEIGHT, 11, DIM, TextAlignment.LEFT, top).Text = "Entities\nGPU memory\nLua memory\nResolution\nRender scale\nThermal\nCPU load"
    ui.right = Label(detail, right, y, COLUMN, COUNTER_HEIGHT, 11, TEXT, TextAlignment.RIGHT, top)
    y = y + COUNTER_HEIGHT + 6

    scratch.x, scratch.y = WIDTH, y
    detail.Size = scratch
end

local function SetMode(newMode)
    mode = newMode
    if mode == "Off" then
        setRoot(nil) -- Out of the draw list entirely
        return
    end

    if not ui then Build() end
    ui.detail.Visible = mode == "Detailed"
    textTimer = TEXT_INTERVAL
    setRoot(ui.root)
end

local function UpdateGraph(target)
    local bar = ui.bars[graphHead]
    local ms = stats.LastFrameMs
    bar.FillAmount = min(ms / (target * 2), 1)
    bar.Color = ms <= target * 1.05 and BAR or BAR_OVER
    graphHead = graphHead % GRAPH_BARS + 1
    ui.bars[graphHead].FillAmount = 0 -- Gap in front of the write head
end

local function UpdateGroups()
    local total = max(stats.GPUWorkMs, 0.0001)
    for r = 1, GROUP_ROWS do
        local row = ui.rows[r]
        local group = r <= stats.GroupCount and stats.Groups[r] or nil
        if group then
            row.name.Text = group.Name
            row.ms.Text = format("%.2f ms", group.Ms)
            row.share.Text = format("%d%%", floor(group.Ms / total * 100 + 0.5))
        else
            row.name.Text = (r == 1 and not stats.GPUTiming) and "GPU timing unavailable" or ""
            row.ms.Text = ""
            row.share.Text = ""
        end
    end
end

local function UpdateText(target)
    local frameColor = FrameColor(stats.FrameMs, target)
    ui.fps.Text = format("%d FPS", floor(stats.FPS + 0.5))
    ui.fps.Color = frameColor
    ui.cpu.Text = format("CPU %.1f ms", stats.CPUWorkMs)
    ui.cpu.Color = BudgetColor(stats.CPUWorkMs, target)
    if stats.GPUTiming then
        ui.gpu.Text = format("GPU %.1f ms", stats.GPUWorkMs)
        ui.gpu.Color = BudgetColor(stats.GPUWorkMs, target)
    else
        ui.gpu.Text = "GPU n/a"
        ui.gpu.Color = DIM
    end

    if mode ~= "Detailed" then return end

    ui.cpuName.Text = stats.CPUName ~= "" and stats.CPUName or "Unknown"
    ui.gpuName.Text = stats.GPUName
    ui.frame.Text = format("%.1f ms  peak %.1f", stats.FrameMs, stats.FrameMaxMs)
    ui.frame.Color = frameColor
    ui.budget.Text = format("TARGET %.1f MS  %d HZ", target, floor(1000 / target + 0.5))
    -- Some drivers (Adreno) wait for the image before the command buffer starts: no wait to show there
    local wait = stats.GPUTiming and stats.GPUDisplayWaitMs or 0
    ui.gpuTitle.Text = wait > 0.05 and format("GPU WORK  (DISPLAY WAIT %.2f MS)", wait) or "GPU WORK"
    UpdateGroups()
    ui.left.Text = format("%d / %s\n%d\n%d / %d\n%s / %s\n%d\n%d\n%d", stats.DrawCalls, Count(stats.Triangles), stats.Meshes, stats.Lights, stats.MaxLights,
                          Count(stats.Particles), Count(stats.MaxParticles), stats.UIItems, stats.Animators, stats.CPUThreads)
    ui.right.Text = format("%d\n%d / %d MB\n%.1f MB\n%dx%d\n%.2fx\n%s\n%s", stats.Entities, stats.GPUMemoryMB, stats.GPUMemoryBudgetMB,
                           collectgarbage("count") / 1024, stats.RenderWidth, stats.RenderHeight, stats.RenderScale,
                           stats.Thermal ~= "" and stats.Thermal or "n/a",
                           stats.CPUUsage >= 0 and format("%d%%", floor(stats.CPUUsage + 0.5)) or "--") -- The first second has no sample yet
end

local function Update(dt)
    if not ui or mode == "Off" then return end

    Diagnostics.GetStats(stats)
    local target = stats.TargetMs
    if mode == "Detailed" then UpdateGraph(target) end

    textTimer = textTimer + dt
    if textTimer < TEXT_INTERVAL then return end
    textTimer = 0
    UpdateText(target)
end

return { SetMode = SetMode, Update = Update }
)lua";

    static constexpr Uint GESTURE_TAPS = 3;
    static constexpr float TAP_MAX_DURATION = 0.35f; // Seconds from the first finger down to the last one up
    static constexpr float TAP_MAX_GAP = 0.4f;       // Seconds between two taps of the gesture
    static constexpr float TAP_SLOP = 0.04f;         // Movement allowed per finger, fraction of the window's shorter side

    struct PerformanceOverlayData
    {
        sol::protected_function LuaSetMode;
        sol::protected_function LuaUpdate;
        PerformanceOverlayMode Mode = PerformanceOverlayMode::OFF;
        bool HotkeyEnabled = true;

        // Two finger triple tap
        float TapTime = 0.0f; // Seconds the current tap lasted
        float TapGap = 0.0f;  // Seconds since the last tap ended
        Uint TapFingers = 0;  // Most fingers down at once during the current tap
        Uint TapCount = 0;
        bool InTap = false;
        bool TapValid = false;
    };
    static PerformanceOverlayData* sData = nullptr;

    static void ReportLuaError(const sol::protected_function_result& result)
    {
        sol::error error = result;
        Log<Severity::Error>("[PerformanceOverlay] {}", error.what());
    }

    // Two finger triple tap: the hotkey of touch screens.
    static void UpdateGesture(PerformanceOverlayData& data, float deltaTime)
    {
        if(!data.HotkeyEnabled)
        {
            data.InTap = false;
            data.TapCount = 0;
            return;
        }

        const glm::vec2 windowSize = Core::GetWindow()->GetSize();
        const float slop = std::min(windowSize.x, windowSize.y) * TAP_SLOP;

        Uint fingers = 0;
        bool valid = true;
        const Uint touchCount = Input::GetTouchCount();
        for(Uint i = 0; i < touchCount; i++)
        {
            const Touch& touch = Input::GetTouch(i);
            if(touch.IsMouse)
                continue;

            if(touch.Phase != TouchPhase::ENDED && touch.Phase != TouchPhase::CANCELED)
                fingers++;
            const glm::vec2 moved = touch.Position - touch.StartPosition;
            if(touch.Phase == TouchPhase::CANCELED || moved.x * moved.x + moved.y * moved.y > slop * slop)
                valid = false;
        }

        if(!data.InTap)
        {
            if(fingers == 0)
            {
                if(data.TapCount > 0)
                {
                    data.TapGap += deltaTime;
                    if(data.TapGap > TAP_MAX_GAP)
                        data.TapCount = 0;
                }
                return;
            }

            data.InTap = true;
            data.TapValid = true;
            data.TapTime = 0.0f;
            data.TapFingers = 0;
        }

        data.TapTime += deltaTime;
        data.TapFingers = std::max(data.TapFingers, fingers);
        if(!valid || data.TapFingers > 2 || data.TapTime > TAP_MAX_DURATION)
            data.TapValid = false;
        if(fingers > 0)
            return;

        // Every finger lifted: a tap of the gesture, or any other touch that breaks the sequence
        data.InTap = false;
        data.TapGap = 0.0f;
        if(!data.TapValid || data.TapFingers != 2)
        {
            data.TapCount = 0;
            return;
        }

        if(++data.TapCount == GESTURE_TAPS)
        {
            data.TapCount = 0;
            PerformanceOverlay::Toggle();
        }
    }

    void PerformanceOverlay::Initialize(void* luaState)
    {
        sol::state_view& lua = *static_cast<sol::state_view*>(luaState);
        sData = new PerformanceOverlayData();

        sol::load_result chunk = lua.load(OVERLAY_SCRIPT, "=PerformanceOverlay", sol::load_mode::text);
        if(!chunk.valid())
        {
            sol::error error = chunk;
            Log<Severity::Error>("[PerformanceOverlay] The overlay script does not compile: {}", error.what());
            return;
        }

        sol::protected_function script = chunk;
        sol::environment environment_(lua, sol::create, lua.globals());
        sol::set_environment(environment_, script);

        // The script's only way into the UI system: sets or clears UI::Manager's debug root
        const auto setRoot = [](UI::Widget* root) { Core::GetRenderer()->GetUIManager().SetDebugRoot(root ? Ref<UI::Widget>(root) : nullptr); };
        sol::protected_function_result result = script(sol::as_function(setRoot));
        if(!result.valid())
        {
            ReportLuaError(result);
            return;
        }

        sol::optional<sol::table> api = result;
        if(!api)
        {
            Log<Severity::Error>("[PerformanceOverlay] The overlay script did not return its functions");
            return;
        }
        sData->LuaSetMode = (*api)["SetMode"];
        sData->LuaUpdate = (*api)["Update"];
    }

    void PerformanceOverlay::Shutdown()
    {
        if(!sData)
            return;

        Core::GetRenderer()->GetUIManager().SetDebugRoot(nullptr);

        delete sData; // Releases the Lua references while the state is still open
        sData = nullptr;
    }

    void PerformanceOverlay::OnEvent(Event& e)
    {
        if(!sData || !sData->HotkeyEnabled)
            return;

        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<KeyPressedEvent>([](KeyPressedEvent& keyEvent) {
            // Auto-repeat: InputState's edge (it already saw this event), the repeat count is 1 for every press on Windows
            if(keyEvent.GetKeyCode() != Key::F3 || !Input::WasKeyPressed(Key::F3))
                return;
            if(!Input::IsKeyPressed(Key::LeftControl) && !Input::IsKeyPressed(Key::RightControl))
                return;

            Toggle();
            keyEvent.Handled = true;
        });
    }

    void PerformanceOverlay::Update()
    {
        if(!sData)
            return;

        SURGE_PROFILE_FUNC("PerformanceOverlay::Update");
        const float deltaTime = Core::GetClock().GetSeconds();
        UpdateGesture(*sData, deltaTime);

        if(sData->Mode != PerformanceOverlayMode::OFF && sData->LuaUpdate.valid())
        {
            sol::protected_function_result result = sData->LuaUpdate(deltaTime);
            if(!result.valid())
            {
                ReportLuaError(result);
                SetMode(PerformanceOverlayMode::OFF); // One error in the log instead of one per frame
            }
        }
    }

    void PerformanceOverlay::SetMode(PerformanceOverlayMode mode)
    {
        if(!sData || sData->Mode == mode)
            return;

        sData->Mode = mode;

        // Diagnostics measures (GPU timestamps included) while the script keeps reading it, starting now
        if(mode != PerformanceOverlayMode::OFF)
            Core::GetDiagnostics()->Request();

        if(sData->LuaSetMode.valid())
        {
            sol::protected_function_result result = sData->LuaSetMode(ModeToString(mode));
            if(!result.valid())
            {
                ReportLuaError(result);
                Core::GetRenderer()->GetUIManager().SetDebugRoot(nullptr);
            }
        }
    }

    PerformanceOverlayMode PerformanceOverlay::GetMode()
    {
        return sData ? sData->Mode : PerformanceOverlayMode::OFF;
    }

    void PerformanceOverlay::Toggle()
    {
        switch(GetMode())
        {
            case PerformanceOverlayMode::OFF:      SetMode(PerformanceOverlayMode::COMPACT); break;
            case PerformanceOverlayMode::COMPACT:  SetMode(PerformanceOverlayMode::DETAILED); break;
            case PerformanceOverlayMode::DETAILED: SetMode(PerformanceOverlayMode::OFF); break;
        }
    }

    void PerformanceOverlay::SetHotkeyEnabled(bool enabled)
    {
        if(sData)
            sData->HotkeyEnabled = enabled;
    }

    bool PerformanceOverlay::IsHotkeyEnabled()
    {
        return sData && sData->HotkeyEnabled;
    }

    const char* PerformanceOverlay::ModeToString(PerformanceOverlayMode mode)
    {
        switch(mode)
        {
            case PerformanceOverlayMode::OFF:      return "Off";
            case PerformanceOverlayMode::COMPACT:  return "Compact";
            case PerformanceOverlayMode::DETAILED: return "Detailed";
        }
        return "Off";
    }

    bool PerformanceOverlay::ModeFromString(const String& name, PerformanceOverlayMode& outMode)
    {
        const auto equals = [&name](const char* other) {
            return std::equal(name.begin(), name.end(), other, other + std::char_traits<char>::length(other),
                              [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)); });
        };

        for(PerformanceOverlayMode mode : { PerformanceOverlayMode::OFF, PerformanceOverlayMode::COMPACT, PerformanceOverlayMode::DETAILED })
        {
            if(equals(ModeToString(mode)))
            {
                outMode = mode;
                return true;
            }
        }
        return false;
    }

} // namespace Surge
