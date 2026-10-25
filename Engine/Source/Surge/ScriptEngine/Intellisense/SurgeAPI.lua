---@meta SurgeAPI

-- ==============================================================================
-- SURGE ENGINE: MASTER INTELLISENSE FILE
-- Do NOT Add this file in ScriptComponent, drop this file in your Assets/Scripts
-- directory and VSCode should automatically provide you with intellisense. You 
-- need to have the VSCode Lua extension by sumneko installed
-- https://marketplace.visualstudio.com/items?itemName=sumneko.lua
-- ==============================================================================

-- ==============================================================================
-- SCRIPT STRUCTURE, MODULES & PROPERTIES
-- ==============================================================================
-- An entity script defines any of these (all optional), each runs with the owning entity:
--   function OnCreate(entity) end                    -- once, the frame the script starts
--   function OnEnable(entity) end                    -- after OnCreate, and whenever it is enabled again
--   function OnDisable(entity) end                   -- when disabled, and before OnDestroy
--   function OnFixedUpdate(entity, fixedDt) end      -- before every physics step (1/60 s), forces go here
--   function OnUpdate(entity, deltaTime) end         -- every frame
--   function OnLateUpdate(entity, deltaTime) end     -- every frame, after every OnUpdate (cameras, follow logic)
--   function OnDestroy(entity) end                   -- entity destroyed, scene unloaded or Play stopped
--   function OnCollisionEnter(entity, other) end     -- physics contact begins / ends
--   function OnCollisionExit(entity, other) end
--   function OnTriggerEnter(entity, other) end       -- same, when one of the bodies is a sensor (IsSensor)
--   function OnTriggerExit(entity, other) end        -- 'other' may be destroyed already: check other:IsValid()
-- Frame order: fixed steps (OnFixedUpdate) -> contact events -> OnUpdate -> new scripts' OnCreate/OnEnable
--              -> UI canvas scripts -> OnLateUpdate.
-- An entity can have any number of scripts (Script Component), each with its own globals and Properties;
-- they never leak into other scripts. Toggle one with entity.ScriptC:SetScriptActive("Name", false).
--
-- MODULES (share code and state without _G)
--   local State = require("Scripts.Game.State")  -- Assets/Scripts/Game/State.lua
--   "Scripts/Game/State" and "Scripts/Game/State.lua" work too, and a unique file name alone
--   ("State") is found anywhere in the project. A module runs once per play session and every
--   script that requires it gets the same value, so a table returned by a module is shared state.
--   A module that returns nothing exports its globals. Only script assets can be required
--   (no files, no native modules), so it works the same in the Editor and on Android.
--   The Editor resets modules and _G whenever Play starts or stops.
--
-- PROPERTIES (edited per entity in the Inspector)
--   Properties = {
--       Speed  = 6.5,                                   -- FLOAT (plain values work: number, boolean,
--       Lives  = 3,                                     -- INT    string, Math.Vec2/Vec3/Vec4)
--       Jump   = Property.Float(2.0, 0, 10, "Jump height in meters"), -- range + tooltip
--       Tint   = Property.Color(1, 0.5, 0, 1),
--       Target = Property.Entity("Who to follow"),     -- picked by dropping an entity from the Hierarchy
--   }
--   function OnUpdate(entity, dt) local speed = Properties.Speed end
-- Properties holds plain values at runtime: the Inspector values are written into it once, before
-- OnCreate, so reading Properties.X costs the same as any table field. Top level code runs before
-- that, so read Properties inside the callbacks. An Entity property is nil when nothing is assigned.

--- Typed declarations for the Properties table. At runtime they just return the value, the extra
--- arguments (range, tooltip) are only read by the Editor.
---@class Property
Property = {}

---@param value number|nil Default (0)
---@param min number|nil Range minimum, used when min < max
---@param max number|nil Range maximum
---@param tooltip string|nil
---@return number
function Property.Float(value, min, max, tooltip) end

---@param value integer|nil Default (0)
---@param min integer|nil Range minimum, used when min < max
---@param max integer|nil Range maximum
---@param tooltip string|nil
---@return integer
function Property.Int(value, min, max, tooltip) end

---@param value boolean|nil Default (false)
---@param tooltip string|nil
---@return boolean
function Property.Bool(value, tooltip) end

---@param value string|nil Default ("")
---@param tooltip string|nil
---@return string
function Property.String(value, tooltip) end

---@param tooltip string|nil
---@return Vec2
function Property.Vec2(x, y, tooltip) end

---@param tooltip string|nil
---@return Vec3
function Property.Vec3(x, y, z, tooltip) end

---@param tooltip string|nil
---@return Vec4
function Property.Vec4(x, y, z, w, tooltip) end

--- A Vec4 edited with a color picker (defaults to white).
---@param tooltip string|nil
---@return Vec4
function Property.Color(r, g, b, a, tooltip) end

--- Reference to another entity of the scene, nil when none is assigned.
---@param tooltip string|nil
---@return Entity|nil
function Property.Entity(tooltip) end

--- Reference to an asset, picked by dropping it from the Content Browser. The script gets the asset's path
--- (Assets-relative, e.g. "Meshes/Rock.glb"), ready for MeshC:SetMesh, AudioSourceC:SetAudioClip, Scene.Load...
--- Saved by ID: renaming or moving the asset keeps the reference. nil when none is assigned.
---@param kind string|nil Accepted asset type: "MESH", "TEXTURE2D", "AUDIO", "MATERIAL", "SCRIPT", "FONT", "SCENE", "UI_LAYOUT", "PARTICLE_EFFECT" (nil = any)
---@param path string|nil Default asset path
---@param tooltip string|nil
---@return string|nil
function Property.Asset(kind, path, tooltip) end

--- One string out of a fixed list, edited with a dropdown.
---   Difficulty = Property.Enum("Normal", { "Easy", "Normal", "Hard" })
---@param value string|nil Default (the first option when nil)
---@param options string[]
---@param tooltip string|nil
---@return string
function Property.Enum(value, options, tooltip) end

--- A Lua array of one element type, edited as a list (+ / - buttons).
---   Waypoints = Property.Array("Entity"), Speeds = Property.Array("Float", { 1, 2.5 }), Props = Property.Array("Asset:MESH")
---@param elementType string "Float", "Int", "Bool", "String", "Vec2", "Vec3", "Vec4", "Color", "Entity", "Asset" or "Asset:<KIND>"
---@param values table|nil Default elements
---@param tooltip string|nil
---@return table
function Property.Array(elementType, values, tooltip) end

-- ==============================================================================
-- TIME, SCENE, PHYSICS QUERIES & SAVE DATA
-- ==============================================================================
---@class Time
Time = {}
--- Seconds since the last frame, scaled by the time scale (also the deltaTime OnUpdate receives).
---@return number
function Time.GetDeltaTime() end
---@return number
function Time.GetUnscaledDeltaTime() end
--- Scaled seconds since the play session started.
---@return number
function Time.GetTime() end
---@return number
function Time.GetUnscaledTime() end
---@return integer
function Time.GetFrameCount() end
--- The fixed physics step (1 / the project's physics Fixed Rate, 1/60 s by default), what OnFixedUpdate receives.
---@return number
function Time.GetFixedDeltaTime() end
---@return number
function Time.GetTimeScale() end
--- 0 freezes the game (physics and script delta time), 0.5 is slow motion, 1 is normal. Rendering, input and audio keep running.
---@param scale number
function Time.SetTimeScale(scale) end

---@class Scene
Scene = {}
--- First entity with this name in the running scene, nil if there is none.
---@param name string
---@return Entity|nil
function Scene.FindEntity(name) end
---@param name string
---@return Entity|nil
function Scene.CreateEntity(name) end
--- Switches to a fresh copy of a scene asset at the end of the frame. Modules, _G and Storage survive the switch,
--- the old scene's scripts get OnDisable/OnDestroy. Returns false if the path is not a scene asset.
---   Scene.Load("Scenes/Level2.srg")   -- or a Property.Asset("SCENE") value
---@param scenePath string
---@return boolean
function Scene.Load(scenePath) end

---@class RaycastHit
---@field Entity Entity|nil
---@field Point Vec3
---@field Normal Vec3
---@field Distance number

---@class Physics
Physics = {}
--- Closest body along a ray (sensors included). Only bodies of the running scene exist (Play mode).
---@param origin Vec3
---@param direction Vec3 Does not need to be normalized
---@param maxDistance number
---@param layerMask? integer Only bodies on these layers are hit (Physics.GetLayerMask), every layer when omitted
---@return RaycastHit|nil
function Physics.Raycast(origin, direction, maxDistance, layerMask) end
--- Index (0-15) of the layer named in the Physics panel, -1 when no layer has that name.
---@param name string
---@return integer
function Physics.GetLayer(name) end
--- Name of a layer, "" when it is unused.
---@param layer integer
---@return string
function Physics.GetLayerName(layer) end
--- Raycast mask of the named layers, e.g. Physics.GetLayerMask("Default", "Ground"). Cache it, it does not change at runtime.
---@param ... string
---@return integer
function Physics.GetLayerMask(...) end
--- Whether the collision matrix lets bodies on these two layers collide.
---@param layerA integer
---@param layerB integer
---@return boolean
function Physics.ShouldCollide(layerA, layerB) end

--- Persistent save data, survives app restarts (Android internal storage, %APPDATA%/SurgeEngine/<Project> on Windows).
--- Values: nil, boolean, number, string, Math.Vec2/Vec3/Vec4 and tables of those. Keys: 1-64 of a-z A-Z 0-9 _ - .
--- File IO: save on events (level done, settings changed), not every frame.
---@class Storage
Storage = {}
---@param key string
---@param value any
---@return boolean
function Storage.Save(key, value) end
---@param key string
---@param default any Returned when nothing is saved under key
---@return any
function Storage.Load(key, default) end
---@param key string
---@return boolean
function Storage.Has(key) end
---@param key string
---@return boolean
function Storage.Delete(key) end

--- Diagnostics: live performance numbers, the frame pacing profiling switch and the performance overlay (Ctrl+F3, or a two finger
--- triple tap on a touch screen: Off -> Compact -> Detailed -> Off). Timings are averaged over 0.5 s. Measuring runs only while
--- something reads it: each GetStats call keeps it (and the GPU timestamps) running for a second, otherwise it costs nothing.
---@class DiagnosticsGroup
---@field Name string Render graph group (MainScene, Shadow, PostProcess...)
---@field Ms number Average GPU milliseconds

---@class DiagnosticsStats
---@field FPS number
---@field FrameMs number Frame time: CPU work plus every wait (GPU, swapchain, frame pacing, frame cap)
---@field FrameMaxMs number
---@field CPUWorkMs number The frame's own CPU time, without those waits
---@field CPUWorkMaxMs number
---@field GPUWorkMs number GPU work (sum of the timed scopes, not the swapchain wait)
---@field GPUWorkMaxMs number
---@field GPUCommandBufferMs number Whole GPU command buffer, includes the swapchain wait
---@field GPUDisplayWaitMs number GPU idle until the display released the swapchain image (vsync), not part of GPUWorkMs
---@field LastFrameMs number Last frame, not averaged
---@field LastGPUWorkMs number Last resolved GPU frame, not averaged
---@field GPUTiming boolean The GPU fields are valid
---@field TargetMs number 1000 / frame cap (60 Hz when uncapped)
---@field DrawCalls integer
---@field Triangles integer Triangles drawn by every pass of the frame (shadow casters count again)
---@field Meshes integer Submitted meshes (before culling)
---@field Lights integer
---@field MaxLights integer
---@field Sprites integer
---@field Texts integer
---@field UIItems integer UI sprites + text blocks
---@field Particles integer Alive particles
---@field MaxParticles integer
---@field Animators integer
---@field Entities integer Entities in the running scene
---@field GPUMemoryMB integer
---@field GPUMemoryBudgetMB integer
---@field GPUName string
---@field CPUName string CPU model without vendor marks and clock ("Intel Core i7-10750H", Android 12+: "QTI SM6115"), "" when unknown
---@field CPUThreads integer Logical CPU cores
---@field CPUUsage number Percent of every CPU thread the game used over the last second (0-100), -1 until the first second passed
---@field ScreenWidth integer
---@field ScreenHeight integer
---@field RenderWidth integer 3D scene resolution (render scale applied)
---@field RenderHeight integer
---@field RenderScale number
---@field Thermal string None, Light, Moderate, Severe, Critical, Emergency, Shutdown ("" when unknown: Windows, Android < 11)
---@field Groups DiagnosticsGroup[] Slowest first
---@field GroupCount integer
---@class Diagnostics
Diagnostics = {}
--- Fills and returns `stats` (a new table without), reuse one table to create no garbage per frame.
---@param stats table|nil
---@return DiagnosticsStats
function Diagnostics.GetStats(stats) end
--- Profiling switch, on by default. Off presents every frame as soon as it is ready (no frame pacing, no frame cap): the GPU stays
--- fully loaded, so GPUWorkMs shows the real cost. Paced, phones lower the GPU clock until the work fills the frame interval.
---@param enabled boolean
function Diagnostics.SetFramePacing(enabled) end
---@return boolean
function Diagnostics.IsFramePacingEnabled() end
--- Off -> Compact -> Detailed -> Off. Bind it to a button where there is no keyboard.
function Diagnostics.ToggleOverlay() end
---@param mode string "Off" | "Compact" | "Detailed"
function Diagnostics.SetOverlay(mode) end
---@return string "Off" | "Compact" | "Detailed"
function Diagnostics.GetOverlay() end
--- Ctrl+F3 and the two finger triple tap, on by default. Lock them in a shipped game, ToggleOverlay/SetOverlay keep working.
---@param enabled boolean
function Diagnostics.SetHotkeyEnabled(enabled) end
---@return boolean
function Diagnostics.IsHotkeyEnabled() end

-- ==============================================================================
-- LOGGING
-- ==============================================================================
---@class Log
Log = {}

--- Logs a trace message.
---@param message string
function Log.Trace(message) end

--- Logs an info message.
---@param message string
function Log.Info(message) end

--- Logs a debug message.
---@param message string
function Log.Debug(message) end

--- Logs a warning message.
---@param message string
function Log.Warn(message) end

--- Logs an error message.
---@param message string
function Log.Error(message) end

-- ==============================================================================
-- INPUT
-- ==============================================================================
---@class Input
Input = {}

--- Checks if a specific keyboard key is currently being held down.
---@param key number
---@return boolean
function Input.IsKeyPressed(key) end

--- Checks if a specific mouse button is currently being held down.
---@param button number
---@return boolean
function Input.IsMouseButtonPressed(button) end

--- Gets the current X and Y position of the mouse cursor.
---@return Vec2
function Input.GetMousePosition() end

--- Gets the current X position of the mouse cursor.
---@return number
function Input.GetMouseX() end

--- Gets the current Y position of the mouse cursor.
---@return number
function Input.GetMouseY() end

--- Sets the cursor mode (e.g., locked, hidden, normal).
---@param cursorMode number
function Input.SetCursorMode(cursorMode) end

--- True only in the frame the key went down / up.
---@param key number
---@return boolean
function Input.WasKeyPressed(key) end
---@param key number
---@return boolean
function Input.WasKeyReleased(key) end

--- True only in the frame the mouse button went down / up (the primary touch also drives Mouse.ButtonLeft).
---@param button number
---@return boolean
function Input.WasMouseButtonPressed(button) end
---@param button number
---@return boolean
function Input.WasMouseButtonReleased(button) end

--- Mouse wheel notches scrolled this frame (+ = away from the user).
---@return number
function Input.GetMouseScrollDelta() end

-- Touch -----------------------------------------------------------------------
-- Positions are window pixels (same space as Input.GetMousePosition), convert them with
-- UICanvasComponent:ScreenToCanvas() to place UI. On desktop the left mouse button is a touch
-- (IsMouse = true) unless Input.SetMouseTouchEmulation(false).

--- Number of touches this frame (including the ones that ENDED/CANCELED this frame).
---@return number
function Input.GetTouchCount() end

--- Touch by 1-based index (1 .. Input.GetTouchCount()), nil if out of range. Returns a copy.
---@param index number
---@return Touch|nil
function Input.GetTouch(index) end

--- Touch by its ID (stable while the finger is down), nil if there is none this frame.
---@param id number
---@return Touch|nil
function Input.GetTouchByID(id) end

--- All touches of this frame as an array.
---@return Touch[]
function Input.GetTouches() end

--- The device has a touch screen.
---@return boolean
function Input.IsTouchSupported() end

--- Left mouse button acts as a touch (default true).
---@param enabled boolean
function Input.SetMouseTouchEmulation(enabled) end
---@return boolean
function Input.IsMouseTouchEmulationEnabled() end

-- Gamepads (XInput on Windows, USB/Bluetooth controllers on Android) ---------------
-- index is 1-based (1 .. Input.MaxGamepads). Sticks: -1..1, +X right, +Y down. Triggers: 0..1.

---@type number
Input.MaxGamepads = 4

---@param index number
---@return boolean
function Input.IsGamepadConnected(index) end
---@param index number
---@param button number GamepadButton
---@return boolean
function Input.IsGamepadButtonPressed(index, button) end
---@param index number
---@param button number GamepadButton
---@return boolean
function Input.WasGamepadButtonPressed(index, button) end
---@param index number
---@param button number GamepadButton
---@return boolean
function Input.WasGamepadButtonReleased(index, button) end
--- Deadzones applied.
---@param index number
---@param axis number GamepadAxis
---@return number
function Input.GetGamepadAxis(index, axis) end

--- Android back button: always reported as Key.Escape. true (default) = it also closes the app,
--- false = the game handles it (e.g. pause / go back a screen).
---@param exits boolean
function Input.SetBackButtonExitsApp(exits) end
---@return boolean
function Input.DoesBackButtonExitApp() end

---@class Touch
---@field ID number Stable while the contact lasts (smallest free index: 0, 1, 2...)
---@field Phase number TouchPhase
---@field Position Vec2 Window pixels
---@field StartPosition Vec2
---@field Delta Vec2 Movement since the previous frame
---@field X number
---@field Y number
---@field Pressure number
---@field Duration number Seconds since the touch began
---@field IsMouse boolean Emulated by the left mouse button
local Touch = {}

--- BEGAN in the first frame, ENDED/CANCELED in the last one (a tap shorter than a frame still gets both).
TouchPhase = {
    BEGAN = 0,
    MOVED = 1,
    STATIONARY = 2,
    ENDED = 3,
    CANCELED = 4
}

GamepadButton = {
    A = 0, B = 1, X = 2, Y = 3,
    LeftShoulder = 4, RightShoulder = 5,
    Back = 6, Start = 7, Guide = 8,
    LeftThumb = 9, RightThumb = 10,
    DPadUp = 11, DPadDown = 12, DPadLeft = 13, DPadRight = 14
}

GamepadAxis = {
    LeftX = 0, LeftY = 1,
    RightX = 2, RightY = 3,
    LeftTrigger = 4, RightTrigger = 5
}

-- ==============================================================================
-- ENUMS & CODES
-- ==============================================================================
Key = {
    Space = 32, Comma = 188, Minus = 189, Period = 190,
    D0 = 48, D1 = 49, D2 = 50, D3 = 51, D4 = 52, D5 = 53, D6 = 54, D7 = 55, D8 = 56, D9 = 57,
    Semicolon = 186,
    A = 65, B = 66, C = 67, D = 68, E = 69, F = 70, G = 71, H = 72, I = 73, J = 74, K = 75, L = 76, M = 77, N = 78, O = 79, P = 80, Q = 81, R = 82, S = 83, T = 84, U = 85, V = 86, W = 87, X = 88, Y = 89, Z = 90,
    LeftBracket = 219, BackSlash = 220, RightBracket = 221, GraveAccent = 222,
    Backspace = 8, Enter = 13, Tab = 9, Pause = 19, NumLock = 144, ScrollLock = 145, CapsLock = 20, Escape = 27,
    PageUp = 33, PageDown = 34, End = 35, Home = 36, Left = 37, Up = 38, Right = 39, Down = 40,
    PrintScreen = 44, Insert = 45, Delete = 46,
    F1 = 112, F2 = 113, F3 = 114, F4 = 115, F5 = 116, F6 = 117, F7 = 118, F8 = 119, F9 = 120, F10 = 121, F11 = 122, F12 = 123, F13 = 124, F14 = 125, F15 = 126, F16 = 127, F17 = 128, F18 = 129, F19 = 130, F20 = 131, F21 = 132, F22 = 133, F23 = 134, F24 = 135,
    KP0 = 96, KP1 = 97, KP2 = 98, KP3 = 99, KP4 = 100, KP5 = 101, KP6 = 102, KP7 = 103, KP8 = 104, KP9 = 105,
    KPMultiply = 106, KPAdd = 107, KPEqual = 108, KPSubtract = 109, KPDecimal = 110, KPDivide = 111,
    LeftShift = 160, RightShift = 161, LeftControl = 162, RightControl = 163, LeftAlt = 164, RightAlt = 165
}

Mouse = {
    ButtonLeft = 1,
    ButtonRight = 2,
    ButtonMiddle = 4
}

RigidbodyType = {
    STATIC = 0,
    DYNAMIC = 1,
    KINEMATIC = 2
}

LightType = {
    DIRECTIONAL = 0,
    POINT = 1
}

RenderProfile = {
    DESKTOP = 0,
    MOBILE = 1
}

ShadowQuality = {
    LOW = 0,
    MEDIUM = 1,
    HIGH = 2,
    ULTRA = 3
}

AOQuality = {
    LOW = 0,
    MEDIUM = 1,
    HIGH = 2,
    ULTRA = 3
}

BloomQuality = {
    LOW = 0,
    MEDIUM = 1,
    HIGH = 2,
    ULTRA = 3
}

TextAlignment = {
    LEFT = 0,
    CENTER = 1,
    RIGHT = 2
}

TextVerticalAlignment = {
    TOP = 0,
    CENTER = 1,
    BOTTOM = 2
}

AttenuationModel = {
    NONE = 0,
    INVERSE_DISTANCE = 1,
    LINEAR_DISTANCE = 2,
    EXPONENTIAL_DISTANCE = 3
}

--- What a particle system does while it is outside the camera (it is never drawn then).
ParticleCullingMode = {
    ALWAYS_SIMULATE = 0,
    PAUSE_OFFSCREEN = 1,   -- Looping effects freeze, one-shots keep running so they finish
    PAUSE_AND_CATCH_UP = 2 -- Like PAUSE_OFFSCREEN, then simulates the missed time (up to 1 s) when visible again
}

-- ==============================================================================
-- MATH (VECTORS, QUATERNIONS & MATRICES)
-- ==============================================================================
-- Vectors are userdata: every operator and every method returning a vector allocates a new one (GC).
-- In hot loops prefer plain numbers, v:Unpack() and v:Set(...) on a reused vector. No method mutates except Set.

---@class Vec2
---@field x number
---@field y number
---@operator add(Vec2): Vec2
---@operator sub(Vec2): Vec2
---@operator mul(number|Vec2): Vec2
---@operator div(number|Vec2): Vec2
---@operator unm: Vec2
local Vec2 = {}
---@param x number
---@param y number
function Vec2:Set(x, y) end
---@return number x, number y
function Vec2:Unpack() end
---@return Vec2
function Vec2:Clone() end
---@return number
function Vec2:Length() end
---@return number
function Vec2:LengthSquared() end
--- Unit length copy, zero vector stays zero
---@return Vec2
function Vec2:Normalize() end
---@param other Vec2
---@return number
function Vec2:Dot(other) end
--- 2D cross product (z of the 3D cross)
---@param other Vec2
---@return number
function Vec2:Cross(other) end
---@param other Vec2
---@return number
function Vec2:Distance(other) end
---@param other Vec2
---@return number
function Vec2:DistanceSquared(other) end
--- Unsigned angle in radians [0, pi]
---@param other Vec2
---@return number
function Vec2:Angle(other) end
--- Signed angle in radians [-pi, pi], positive counter-clockwise
---@param other Vec2
---@return number
function Vec2:SignedAngle(other) end
---@param angleRad number
---@return Vec2
function Vec2:Rotate(angleRad) end
--- Rotated 90 degrees counter-clockwise (-y, x)
---@return Vec2
function Vec2:Perpendicular() end
---@param target Vec2
---@param t number
---@return Vec2
function Vec2:Lerp(target, t) end
---@param target Vec2
---@param maxDistance number
---@return Vec2
function Vec2:MoveTowards(target, maxDistance) end
---@param maxLength number
---@return Vec2
function Vec2:ClampLength(maxLength) end
---@param onto Vec2
---@return Vec2
function Vec2:Project(onto) end
---@param n Vec2
---@return Vec2
function Vec2:Reflect(n) end
---@param n Vec2
---@param eta number
---@return Vec2
function Vec2:Refract(n, eta) end
---@param other Vec2
---@return Vec2
function Vec2:Min(other) end
---@param other Vec2
---@return Vec2
function Vec2:Max(other) end
---@param min Vec2
---@param max Vec2
---@return Vec2
function Vec2:Clamp(min, max) end
---@return Vec2
function Vec2:Abs() end
---@return Vec2
function Vec2:Floor() end
---@return Vec2
function Vec2:Ceil() end
---@return Vec2
function Vec2:Round() end
--- Per component |a - b| <= epsilon (default 1e-6)
---@param other Vec2
---@param epsilon number|nil
---@return boolean
function Vec2:Approximately(other, epsilon) end

---@class Vec3
---@field x number
---@field y number
---@field z number
---@operator add(Vec3): Vec3
---@operator sub(Vec3): Vec3
---@operator mul(number|Vec3): Vec3
---@operator div(number|Vec3): Vec3
---@operator unm: Vec3
local Vec3 = {}
---@param x number
---@param y number
---@param z number
function Vec3:Set(x, y, z) end
---@return number x, number y, number z
function Vec3:Unpack() end
---@return Vec3
function Vec3:Clone() end
---@return Vec2
function Vec3:XY() end
---@return number
function Vec3:Length() end
---@return number
function Vec3:LengthSquared() end
--- Unit length copy, zero vector stays zero
---@return Vec3
function Vec3:Normalize() end
---@param other Vec3
---@return number
function Vec3:Dot(other) end
---@param other Vec3
---@return Vec3
function Vec3:Cross(other) end
---@param other Vec3
---@return number
function Vec3:Distance(other) end
---@param other Vec3
---@return number
function Vec3:DistanceSquared(other) end
--- Unsigned angle in radians [0, pi]
---@param other Vec3
---@return number
function Vec3:Angle(other) end
--- Angle in radians, negative when the rotation from self to other goes clockwise around axis
---@param other Vec3
---@param axis Vec3
---@return number
function Vec3:SignedAngle(other, axis) end
---@param target Vec3
---@param t number
---@return Vec3
function Vec3:Lerp(target, t) end
---@param target Vec3
---@param maxDistance number
---@return Vec3
function Vec3:MoveTowards(target, maxDistance) end
---@param maxLength number
---@return Vec3
function Vec3:ClampLength(maxLength) end
---@param onto Vec3
---@return Vec3
function Vec3:Project(onto) end
---@param normal Vec3
---@return Vec3
function Vec3:ProjectOnPlane(normal) end
---@param n Vec3
---@return Vec3
function Vec3:Reflect(n) end
---@param n Vec3
---@param eta number
---@return Vec3
function Vec3:Refract(n, eta) end
---@param other Vec3
---@return Vec3
function Vec3:Min(other) end
---@param other Vec3
---@return Vec3
function Vec3:Max(other) end
---@param min Vec3
---@param max Vec3
---@return Vec3
function Vec3:Clamp(min, max) end
---@return Vec3
function Vec3:Abs() end
---@return Vec3
function Vec3:Floor() end
---@return Vec3
function Vec3:Ceil() end
---@return Vec3
function Vec3:Round() end
--- Per component |a - b| <= epsilon (default 1e-6)
---@param other Vec3
---@param epsilon number|nil
---@return boolean
function Vec3:Approximately(other, epsilon) end

--- Also used for colors (x, y, z, w = r, g, b, a)
---@class Vec4
---@field x number
---@field y number
---@field z number
---@field w number
---@operator add(Vec4): Vec4
---@operator sub(Vec4): Vec4
---@operator mul(number|Vec4): Vec4
---@operator div(number|Vec4): Vec4
---@operator unm: Vec4
local Vec4 = {}
---@param x number
---@param y number
---@param z number
---@param w number
function Vec4:Set(x, y, z, w) end
---@return number x, number y, number z, number w
function Vec4:Unpack() end
---@return Vec4
function Vec4:Clone() end
---@return Vec2
function Vec4:XY() end
---@return Vec3
function Vec4:XYZ() end
---@return number
function Vec4:Length() end
---@return number
function Vec4:LengthSquared() end
---@return Vec4
function Vec4:Normalize() end
---@param other Vec4
---@return number
function Vec4:Dot(other) end
---@param other Vec4
---@return number
function Vec4:Distance(other) end
---@param other Vec4
---@return number
function Vec4:DistanceSquared(other) end
---@param other Vec4
---@return number
function Vec4:Angle(other) end
---@param target Vec4
---@param t number
---@return Vec4
function Vec4:Lerp(target, t) end
---@param target Vec4
---@param maxDistance number
---@return Vec4
function Vec4:MoveTowards(target, maxDistance) end
---@param maxLength number
---@return Vec4
function Vec4:ClampLength(maxLength) end
---@param onto Vec4
---@return Vec4
function Vec4:Project(onto) end
---@param n Vec4
---@return Vec4
function Vec4:Reflect(n) end
---@param n Vec4
---@param eta number
---@return Vec4
function Vec4:Refract(n, eta) end
---@param other Vec4
---@return Vec4
function Vec4:Min(other) end
---@param other Vec4
---@return Vec4
function Vec4:Max(other) end
---@param min Vec4
---@param max Vec4
---@return Vec4
function Vec4:Clamp(min, max) end
---@return Vec4
function Vec4:Abs() end
---@return Vec4
function Vec4:Floor() end
---@return Vec4
function Vec4:Ceil() end
---@return Vec4
function Vec4:Round() end
---@param other Vec4
---@param epsilon number|nil
---@return boolean
function Vec4:Approximately(other, epsilon) end

---@class Quat
---@field x number
---@field y number
---@field z number
---@field w number
---@operator mul(Quat): Quat
---@operator mul(Vec3): Vec3
local Quat = {}
---@param x number
---@param y number
---@param z number
---@param w number
function Quat:Set(x, y, z, w) end
---@return number x, number y, number z, number w
function Quat:Unpack() end
---@return Quat
function Quat:Clone() end
--- Euler angles in radians
---@return Vec3
function Quat:Euler() end
---@return number angleRad, Vec3 axis
function Quat:ToAngleAxis() end
---@return Mat4
function Quat:ToMat4() end
---@return Quat
function Quat:Normalize() end
---@return Quat
function Quat:Inverse() end
---@return Quat
function Quat:Conjugate() end
---@param other Quat
---@return number
function Quat:Dot(other) end
--- Angle in radians between the two rotations
---@param other Quat
---@return number
function Quat:Angle(other) end
---@param b Quat
---@param t number
---@return Quat
function Quat:Slerp(b, t) end
--- Normalized linear interpolation (cheaper than Slerp, shortest path)
---@param b Quat
---@param t number
---@return Quat
function Quat:Lerp(b, t) end
---@param target Quat
---@param maxAngleRad number
---@return Quat
function Quat:RotateTowards(target, maxAngleRad) end
--- Same as q * v
---@param v Vec3
---@return Vec3
function Quat:Rotate(v) end
--- Rotated -Z (the engine's forward)
---@return Vec3
function Quat:Forward() end
---@return Vec3
function Quat:Right() end
---@return Vec3
function Quat:Up() end

--- Column major like GLSL: Get(column, row), indices 0-3, translation is column 3.
---@class Mat4
---@operator mul(Mat4): Mat4
---@operator mul(Vec4): Vec4
local Mat4 = {}
--- nil when out of range
---@param column integer
---@param row integer
---@return number|nil
function Mat4:Get(column, row) end
--- Ignored when out of range
---@param column integer
---@param row integer
---@param value number
function Mat4:Set(column, row, value) end
---@param column integer
---@return Vec4|nil
function Mat4:GetColumn(column) end
---@param column integer
---@param value Vec4
function Mat4:SetColumn(column, value) end
---@param v Vec3
---@return Mat4
function Mat4:Translate(v) end
---@param angleRad number
---@param axis Vec3
---@return Mat4
function Mat4:Rotate(angleRad, axis) end
---@param v Vec3
---@return Mat4
function Mat4:Scale(v) end
---@return Mat4
function Mat4:Inverse() end
---@return Mat4
function Mat4:Transpose() end
---@return number
function Mat4:Determinant() end
---@return Mat4
function Mat4:Clone() end
--- Position (w = 1, divided by w for projections)
---@param p Vec3
---@return Vec3
function Mat4:TransformPoint(p) end
--- Direction (w = 0, translation ignored)
---@param d Vec3
---@return Vec3
function Mat4:TransformDirection(d) end
---@return Vec3 position, Quat rotation, Vec3 scale
function Mat4:Decompose() end

-- ==============================================================================
-- MATH (FUNCTIONS)
-- ==============================================================================
-- Scalar functions are plain C functions on doubles, exactly as fast as Lua's own math library (which stays available).
-- Floor/Ceil/Round/Trunc/Min/Max/Clamp return integers for integer input like math.floor/math.min.
-- Calling through a local (local sin = Math.Sin) skips two table lookups per call.

---@class Math
Math = {
    Pi = 3.141592653589793,
    Tau = 6.283185307179586,
    Epsilon = 1.1920929e-07, -- float epsilon
    Infinity = math.huge,
    Deg2Rad = 0.017453292519943295,
    Rad2Deg = 57.29577951308232,
    MaxInteger = math.maxinteger,
    MinInteger = math.mininteger,
    Vec2 = {},
    Vec3 = {},
    Vec4 = {},
    Quat = {},
    Mat4 = {},
    Easing = {}
}

---@overload fun(): Vec2
---@overload fun(scalar: number): Vec2
---@param x number
---@param y number
---@return Vec2
function Math.Vec2.new(x, y) end

---@overload fun(): Vec3
---@overload fun(scalar: number): Vec3
---@overload fun(xy: Vec2, z: number): Vec3
---@param x number
---@param y number
---@param z number
---@return Vec3
function Math.Vec3.new(x, y, z) end

---@overload fun(): Vec4
---@overload fun(scalar: number): Vec4
---@overload fun(xyz: Vec3, w: number): Vec4
---@param x number
---@param y number
---@param z number
---@param w number
---@return Vec4
function Math.Vec4.new(x, y, z, w) end

--- Constructs a matrix. No args for a zero matrix, or a single number for
--- a diagonal matrix (e.g. Math.Mat4.new(1.0) for identity).
---@overload fun(): Mat4
---@overload fun(diagonal: number): Mat4
function Math.Mat4.new() end

--- Returns the identity matrix.
---@return Mat4
function Math.Mat4.Identity() end

--- Translation * Rotation * Scale
---@param position Vec3
---@param rotation Quat
---@param scale Vec3
---@return Mat4
function Math.Mat4.TRS(position, rotation, scale) end

---@param fovYRad number
---@param aspect number
---@param near number
---@param far number
---@return Mat4
function Math.Mat4.Perspective(fovYRad, aspect, near, far) end

---@return Mat4
function Math.Mat4.Orthographic(left, right, bottom, top, near, far) end

--- View matrix
---@param eye Vec3
---@param target Vec3
---@param up Vec3
---@return Mat4
function Math.Mat4.LookAt(eye, target, up) end

--- Constructs a quaternion. No args for identity, (x, y, z, w) components,
--- or a single Vec3 of euler angles (radians).
---@overload fun(): Quat
---@overload fun(x: number, y: number, z: number, w: number): Quat
---@overload fun(euler: Vec3): Quat
function Math.Quat.new() end

---@return Quat
function Math.Quat.Identity() end

---@param eulerRad Vec3
---@return Quat
function Math.Quat.FromEuler(eulerRad) end

---@param angleRad number
---@param axis Vec3
---@return Quat
function Math.Quat.AngleAxis(angleRad, axis) end

--- Shortest rotation turning from into to
---@param from Vec3
---@param to Vec3
---@return Quat
function Math.Quat.FromToRotation(from, to) end

--- Rotation whose Forward() (-Z) points along dir. up defaults to Math.Up()
---@param dir Vec3
---@param up Vec3|nil
---@return Quat
function Math.Quat.LookRotation(dir, up) end

-- Easing (t in [0, 1], all return 0 at 0 and 1 at 1)
---@param t number
---@return number
function Math.Easing.Linear(t) end
---@param t number
---@return number
function Math.Easing.QuadIn(t) end
---@param t number
---@return number
function Math.Easing.QuadOut(t) end
---@param t number
---@return number
function Math.Easing.QuadInOut(t) end
---@param t number
---@return number
function Math.Easing.CubicIn(t) end
---@param t number
---@return number
function Math.Easing.CubicOut(t) end
---@param t number
---@return number
function Math.Easing.CubicInOut(t) end
---@param t number
---@return number
function Math.Easing.SineIn(t) end
---@param t number
---@return number
function Math.Easing.SineOut(t) end
---@param t number
---@return number
function Math.Easing.SineInOut(t) end
---@param t number
---@return number
function Math.Easing.ExpoIn(t) end
---@param t number
---@return number
function Math.Easing.ExpoOut(t) end
--- Pulls back below 0 before going to 1
---@param t number
---@return number
function Math.Easing.BackIn(t) end
--- Overshoots 1 then settles
---@param t number
---@return number
function Math.Easing.BackOut(t) end
---@param t number
---@return number
function Math.Easing.ElasticOut(t) end
---@param t number
---@return number
function Math.Easing.BounceIn(t) end
---@param t number
---@return number
function Math.Easing.BounceOut(t) end

-- Trigonometry (radians)
---@param rad number
---@return number
function Math.Sin(rad) end
---@param rad number
---@return number
function Math.Cos(rad) end
---@param rad number
---@return number
function Math.Tan(rad) end
---@param x number
---@return number
function Math.Asin(x) end
---@param x number
---@return number
function Math.Acos(x) end
--- Atan(y) or Atan(y, x) like math.atan
---@param y number
---@param x number|nil
---@return number
function Math.Atan(y, x) end
---@param y number
---@param x number
---@return number
function Math.Atan2(y, x) end
---@param deg number
---@return number
function Math.ToRadians(deg) end
---@param rad number
---@return number
function Math.ToDegrees(rad) end

-- Exponents
---@param val number
---@return number
function Math.Sqrt(val) end
---@param base number
---@param exp number
---@return number
function Math.Pow(base, exp) end
---@param x number
---@return number
function Math.Exp(x) end
--- Natural log, or log in the given base
---@param x number
---@param base number|nil
---@return number
function Math.Log(x, base) end

-- Rounding
---@param val number
---@return number
function Math.Abs(val) end
--- -1, 0 or 1
---@param val number
---@return integer
function Math.Sign(val) end
---@param val number
---@return integer|number
function Math.Floor(val) end
---@param val number
---@return integer|number
function Math.Ceil(val) end
--- Halves round away from zero
---@param val number
---@return integer|number
function Math.Round(val) end
--- Towards zero
---@param val number
---@return integer|number
function Math.Trunc(val) end
--- val - Floor(val)
---@param val number
---@return number
function Math.Fract(val) end

-- Min / Max / Clamp
---@param ... number
---@return number
function Math.Min(...) end
---@param ... number
---@return number
function Math.Max(...) end
---@param val number
---@param min number
---@param max number
---@return number
function Math.Clamp(val, min, max) end
---@param val number
---@return number
function Math.Clamp01(val) end

-- Interpolation
--- Unclamped
---@param a number
---@param b number
---@param t number
---@return number
function Math.Lerp(a, b, t) end
--- Where value sits between a and b (unclamped, 0 when a == b)
---@param a number
---@param b number
---@param value number
---@return number
function Math.InverseLerp(a, b, value) end
--- Maps value from [inMin, inMax] to [outMin, outMax] (unclamped)
---@return number
function Math.Remap(value, inMin, inMax, outMin, outMax) end
---@param edge0 number
---@param edge1 number
---@param x number
---@return number
function Math.SmoothStep(edge0, edge1, x) end
--- 0 when x < edge, else 1
---@param edge number
---@param x number
---@return number
function Math.Step(edge, x) end
--- Wraps t into [0, length)
---@param t number
---@param length number
---@return number
function Math.Repeat(t, length) end
--- Bounces t between 0 and length
---@param t number
---@param length number
---@return number
function Math.PingPong(t, length) end
---@param current number
---@param target number
---@param maxDelta number
---@return number
function Math.MoveTowards(current, target, maxDelta) end
--- Shortest signed difference between two angles (radians)
---@param fromRad number
---@param toRad number
---@return number
function Math.DeltaAngle(fromRad, toRad) end
--- Lerp along the shortest way around the circle (radians)
---@param aRad number
---@param bRad number
---@param t number
---@return number
function Math.LerpAngle(aRad, bRad, t) end
--- Critically damped spring towards target. Keep the returned velocity and pass it back next frame:
--- x, vel = Math.SmoothDamp(x, target, vel, 0.3, dt)
---@param current number
---@param target number
---@param velocity number
---@param smoothTime number
---@param deltaTime number
---@param maxSpeed number|nil
---@return number value, number velocity
function Math.SmoothDamp(current, target, velocity, smoothTime, deltaTime, maxSpeed) end
--- |a - b| <= epsilon * max(1, |a|, |b|), epsilon defaults to 1e-6
---@param a number
---@param b number
---@param epsilon number|nil
---@return boolean
function Math.Approximately(a, b, epsilon) end

-- Random (own generator: Math.SetSeed makes runs reproducible without touching math.random)
--- [0, 1)
---@return number
function Math.Random() end
--- [min, max)
---@param min number
---@param max number
---@return number
function Math.RandomRange(min, max) end
--- Integer in [min, max], both inclusive
---@param min integer
---@param max integer
---@return integer
function Math.RandomInt(min, max) end
---@param seed integer
function Math.SetSeed(seed) end
---@return Vec2
function Math.RandomInsideUnitCircle() end
---@return Vec3
function Math.RandomOnUnitSphere() end
---@return Vec3
function Math.RandomInsideUnitSphere() end

-- Vector Math
---@param a Vec3
---@param b Vec3
---@param t number
---@return Vec3
function Math.LerpVec3(a, b, t) end

-- Directional Vectors (forward is -Z)
---@return Vec3
function Math.Up() end
---@return Vec3
function Math.Down() end
---@return Vec3
function Math.Right() end
---@return Vec3
function Math.Left() end
---@return Vec3
function Math.Forward() end
---@return Vec3
function Math.Back() end

-- Colors
--- hsv.x = hue in degrees (0-360), y = saturation, z = value
---@param hsv Vec3
---@return Vec3
function Math.HSVToRGB(hsv) end

--- Returns hue in degrees (0-360), saturation, value
---@param rgb Vec3
---@return Vec3
function Math.RGBToHSV(rgb) end

--- "#RRGGBB" / "RRGGBB" (alpha digits ignored). White when invalid
---@param hex string
---@return Vec3
function Math.HexToRGB(hex) end

--- "#RRGGBB" or "#RRGGBBAA" (# optional). White when invalid
---@param hex string
---@return Vec4
function Math.HexToRGBA(hex) end

--- "#RRGGBBAA"
---@param color Vec4
---@return string
function Math.ColorToHex(color) end

-- ==============================================================================
-- UI SYSTEM
-- ==============================================================================

---@class UI
UI = {}

--- Is there an interactable widget (button, ...) of any visible canvas under this window position?
---@param x number
---@param y number
---@return boolean
function UI.IsPointerOverUI(x, y) end

--- Topmost interactable widget of any visible canvas at this window position, or nil.
---@param x number
---@param y number
---@return UIWidget|UIButton|UIText|UIImage|UIImageButton|UIProgressBar|UIToggle|UISlider|UIJoystick|UIScrollView|UIListView|UILayoutGroup|UIInputField|UIDropdown|UIModal|UITabView|UISpinner|UISafeArea|nil
function UI.WidgetAt(x, y) end

--- Sets the root widget of the UI built in code. Pass nil to clear.
--- Called from a UICanvasComponent script it attaches to that canvas (drawn above its layout), otherwise to a global canvas.
---@param root UIWidget|nil
function SetUIRoot(root) end

--- Rect transform (UI units, scaled by the canvas): AnchorMin/AnchorMax are normalized points in the parent rect,
--- equal = point anchor, different = stretch with the parent on that axis (Size is then added to the stretched size).
---@class UIWidget
---@field Name string
---@field Visible boolean Hidden widgets (and their children) are not drawn and receive no input
---@field Interactable boolean Receives clicks/hover (set automatically by OnClick/OnHoverEnter/OnHoverExit)
---@field Anchor Vec2 Point anchor (sets AnchorMin and AnchorMax)
---@field AnchorMin Vec2
---@field AnchorMax Vec2
---@field Pivot Vec2
---@field Offset Vec2
---@field Size Vec2
---@field Color Vec4
UIWidget = {}

--- Creates a new UIWidget (empty container).
---@return UIWidget
function UIWidget.new() end
--- Adds (or re-parents) a child. Adding the same child twice does nothing.
---@param child UIWidget
function UIWidget:AddChild(child) end
---@param child UIWidget
function UIWidget:RemoveChild(child) end
--- Finds a descendant by name (depth first), returns the concrete type (UIButton, UIText...) or nil.
---@param name string
---@return UIWidget|UIButton|UIText|UIImage|UIImageButton|UIProgressBar|UIToggle|UISlider|UIJoystick|UIScrollView|UIListView|UILayoutGroup|UIInputField|UIDropdown|UIModal|UITabView|UISpinner|UISafeArea|nil
function UIWidget:FindChild(name) end
--- callback(self) receives the clicked widget
---@param callback function
function UIWidget:OnClick(callback) end
---@param callback function
function UIWidget:OnHoverEnter(callback) end
---@param callback function
function UIWidget:OnHoverExit(callback) end
--- callback(self) when the value of a Toggle, Slider, InputField, Dropdown, TabView changes
---@param callback function
function UIWidget:OnValueChanged(callback) end

---@class UIImage : UIWidget
---@field ImageMode UIImageMode SIMPLE, SLICED (9-slice), FILLED
---@field FillAmount number FILLED: 0..1
---@field FillOrigin UIFillOrigin FILLED
---@field PreserveAspect boolean
---@field SliceBorder Vec4 SLICED: left, top, right, bottom in texture pixels
---@field SliceScale number
---@field CornerRadius number UI units, rounded corners without a texture
---@field BorderWidth number UI units
---@field BorderColor Vec4
UIImage = {}
--- Replaces the texture (relative path, "" = plain color).
---@param textureRelPath string
function UIImage:SetTexture(textureRelPath) end

--- Creates a new UIImage from a texture relative path.
---@param textureRelPath string
---@return UIImage
function UIImage.new(textureRelPath) end

--- Text is aligned inside its rect. The default Size of (0, 0) aligns it around the anchor point,
--- a non zero width enables word wrapping at that width.
---@class UIText : UIWidget
---@field Text string
---@field FontSize number
---@field TextAlignment number
---@field TextVAlignment number
---@field WordWrap boolean
---@field Overflow UITextOverflow WRAP, TRUNCATE ("..."), NO_WRAP
UIText = {}

--- Creates a new UIText. Without a font (or with ""), it uses the engine UI font (Roboto SemiCondensed Bold, ASCII).
---@param text string
---@param fontRelPath string|nil
---@return UIText
function UIText.new(text, fontRelPath) end

---@class UIButton : UIImage
---@field NormalColor Vec4
---@field HoverColor Vec4
---@field PressedColor Vec4
UIButton = {}

--- Creates a new UIButton.
---@param text string
---@param fontRelPath string
---@param textureRelPath string
---@return UIButton
function UIButton.new(text, fontRelPath, textureRelPath) end

--- Gets the button's internal text widget.
---@return UIText
function UIButton:GetText() end

---@class UIImageButton : UIImage
---@field NormalColor Vec4
---@field HoverColor Vec4
---@field PressedColor Vec4
UIImageButton = {}

--- Creates a new UIImageButton.
---@param textureRelPath string
---@return UIImageButton
function UIImageButton.new(textureRelPath) end

---@enum UIImageMode
UIImageMode = { SIMPLE = 0, SLICED = 1, FILLED = 2 }
---@enum UIFillOrigin
UIFillOrigin = { LEFT = 0, RIGHT = 1, TOP = 2, BOTTOM = 3 }
---@enum UITextOverflow
UITextOverflow = { WRAP = 0, TRUNCATE = 1, NO_WRAP = 2 }
---@enum UIToggleStyle
UIToggleStyle = { CHECKBOX = 0, SWITCH = 1 }
---@enum UIJoystickMode
UIJoystickMode = { FIXED = 0, FLOATING = 1 }
---@enum UILayoutDirection
UILayoutDirection = { HORIZONTAL = 0, VERTICAL = 1, GRID = 2 }
---@enum UIContentType
UIContentType = { STANDARD = 0, INTEGER = 1, DECIMAL = 2, EMAIL = 3, PHONE = 4, PASSWORD = 5 }

--- The focused text field, or nil.
---@return UIInputField|nil
function UI.GetFocused() end
--- Ends text editing (blurs the focused field, hides the soft keyboard).
function UI.ClearFocus() end
--- Device safe area in UI pixels (x = left, y = top, z = right, w = bottom): notch, status and navigation bars.
---@return Vec4
function UI.GetSafeAreaInsets() end

---@class UIProgressBar : UIWidget
---@field Value number In Min..Max
---@field NormalizedValue number 0..1 (read only)
---@field Min number
---@field Max number
---@field Direction UIFillOrigin Edge the fill grows from
---@field FillPadding number UI units
---@field BackgroundColor Vec4
---@field FillColor Vec4
UIProgressBar = {}
---@return UIProgressBar
function UIProgressBar.new() end

--- Checkbox or switch. Toggles under the same parent with the same Group are radio buttons.
--- OnValueChanged(function(self) ... self.IsOn ... end)
---@class UIToggle : UIWidget
---@field IsOn boolean Setting it fires OnValueChanged
---@field Group string
---@field AllowSwitchOff boolean Grouped toggles: tapping the active one turns it off
---@field Style UIToggleStyle
---@field BoxColor Vec4
---@field CheckColor Vec4
---@field KnobColor Vec4
UIToggle = {}
---@param label string
---@param fontRelPath string
---@return UIToggle
function UIToggle.new(label, fontRelPath) end
---@return UIText
function UIToggle:GetLabel() end

--- OnValueChanged(function(self) ... self.Value ... end) fires while dragging.
---@class UISlider : UIWidget
---@field Value number Setting it fires OnValueChanged
---@field NormalizedValue number 0..1 (read only)
---@field Min number
---@field Max number
---@field WholeNumbers boolean
---@field Direction UIFillOrigin
---@field HandleSize number UI units
---@field TrackThickness number UI units, 0 = whole rect
---@field TrackColor Vec4
---@field FillColor Vec4
---@field HandleColor Vec4
UISlider = {}
---@return UISlider
function UISlider.new() end

--- Virtual stick, read it every frame. Two sticks work at once (each finger presses its own widget).
---@class UIJoystick : UIWidget
---@field Direction Vec2 -1..1 per axis, +Y up, dead zone removed (read only)
---@field Magnitude number 0..1 (read only)
---@field IsActive boolean A finger is on it (read only)
---@field Mode UIJoystickMode FLOATING centers where the finger lands
---@field BaseSize number UI units, 0 = fit the rect
---@field HandleSize number UI units, 0 = 45% of the base
---@field DeadZone number 0..1
---@field HandleRange number Max knob travel in base radii
---@field HideWhenIdle boolean
---@field BaseColor Vec4
---@field KnobColor Vec4
UIJoystick = {}
---@return UIJoystick
function UIJoystick.new() end

--- Busy indicator, Color = dot color.
---@class UISpinner : UIWidget
---@field DotCount number
---@field DotSize number Fraction of the diameter
---@field Speed number Revolutions per second
UISpinner = {}
---@return UISpinner
function UISpinner.new() end

--- Insets its children by the device safe area.
---@class UISafeArea : UIWidget
---@field Edges number Bits: 1 left, 2 top, 4 right, 8 bottom (15 = all)
UISafeArea = {}
---@return UISafeArea
function UISafeArea.new() end

--- Arranges its visible children in a row, column or grid (their Size is used, anchors ignored).
---@class UILayoutGroup : UIWidget
---@field Direction UILayoutDirection
---@field Padding Vec4 left, top, right, bottom
---@field Spacing Vec2
---@field CellSize Vec2 GRID
---@field Columns number GRID, 0 = as many as fit
---@field ChildAlignment Vec2 0 = left/top, 0.5 = center, 1 = right/bottom
---@field ControlChildWidth boolean
---@field ControlChildHeight boolean
---@field FitContent boolean Resize to the content
UILayoutGroup = {}
---@return UILayoutGroup
function UILayoutGroup.new() end

--- Tab bar + one page per child (the child name is the tab label). OnValueChanged fires on tab switches.
---@class UITabView : UIWidget
---@field ActiveTab number 1 based
---@field TabsAtBottom boolean
---@field TabBarHeight number
---@field FontSize number
---@field BarColor Vec4
---@field ActiveTabColor Vec4
---@field TextColor Vec4
---@field ActiveTextColor Vec4
UITabView = {}
---@param fontRelPath string
---@return UITabView
function UITabView.new(fontRelPath) end

--- Clips and scrolls its children (drag with inertia, mouse wheel).
---@class UIScrollView : UIWidget
---@field ScrollPosition Vec2 Normalized 0..1, 0 = top/left
---@field ScrollOffset Vec2 UI units (read only)
---@field ContentSize Vec2 UI units (read only)
---@field Horizontal boolean
---@field Vertical boolean
---@field DecelerationRate number Velocity kept after 1 s of coasting, 0 = no inertia
---@field ShowScrollbars boolean
---@field ScrollbarColor Vec4
UIScrollView = {}
---@return UIScrollView
function UIScrollView.new() end

--- Recycling list: only the visible items exist. The layout's first child is the item template (or use OnCreateItem).
---@class UIListView : UIScrollView
---@field ItemCount number
---@field ItemSize Vec2 0 = template size (width 0 = full width)
---@field Spacing Vec2
---@field Padding Vec4
---@field Columns number
UIListView = {}
---@return UIListView
function UIListView.new() end
--- callback(item, index): fill an item for data index (1 based). Called when items scroll into view.
---@param callback function
function UIListView:OnBindItem(callback) end
--- callback() returns a new item widget, for lists built from code (no layout template).
---@param callback function
function UIListView:OnCreateItem(callback) end
--- Rebinds the visible items (the data changed).
function UIListView:Refresh() end
---@param index number 1 based
function UIListView:ScrollToIndex(index) end

--- Text entry, opens the soft keyboard on Android. OnValueChanged fires on every edit.
---@class UIInputField : UIWidget
---@field Text string
---@field Placeholder string
---@field IsFocused boolean (read only)
---@field ContentType UIContentType
---@field MultiLine boolean
---@field MaxLength number 0 = unlimited
---@field FontSize number
---@field TextColor Vec4
---@field PlaceholderColor Vec4
---@field BackgroundColor Vec4
UIInputField = {}
---@param fontRelPath string
---@return UIInputField
function UIInputField.new(fontRelPath) end
function UIInputField:Focus() end
function UIInputField:Blur() end
--- Enter / keyboard Done. A single line field loses the focus first.
---@param callback function
function UIInputField:OnSubmit(callback) end
---@param callback function
function UIInputField:OnFocus(callback) end
---@param callback function
function UIInputField:OnBlur(callback) end

--- Picks one option, the open list is drawn above everything. OnValueChanged fires on selection.
---@class UIDropdown : UIWidget
---@field Options string[]
---@field SelectedIndex number 1 based, 0 = nothing
---@field SelectedText string (read only)
---@field IsOpen boolean (read only)
---@field Placeholder string
---@field MaxVisibleItems number
---@field FontSize number
---@field TextColor Vec4
---@field BackgroundColor Vec4
UIDropdown = {}
---@param fontRelPath string
---@return UIDropdown
function UIDropdown.new(fontRelPath) end
function UIDropdown:Open() end
function UIDropdown:Close() end

--- Popup above every canvas with a dimmed backdrop that blocks the input below. Its children are its content.
---@class UIModal : UIWidget
---@field IsOpen boolean (read only)
---@field BackdropColor Vec4
---@field CloseOnBackdrop boolean
---@field CloseOnBack boolean Back / Escape closes it
---@field PanelColor Vec4
UIModal = {}
---@return UIModal
function UIModal.new() end
function UIModal:Show() end
function UIModal:Hide() end
---@param callback function
function UIModal:OnClose(callback) end

-- ==============================================================================
-- RENDERER & APPLICATION
-- ==============================================================================

---@class Renderer
Renderer = {}

--- Submits a single debug line to be drawn this frame.
---@param point0 Vec3
---@param point1 Vec3
---@param color Vec4
function Renderer.DrawLine(point0, point1, color) end

--- Submits a single quad to be drawn this frame
---@param Position Vec3
---@param Rotation Vec3
---@param Scale Vec3
---@param color Vec4
function Renderer.DrawQuad(Position, Rotation, Scale, color) end

---@class Application
Application = {}

--- Requests the application to close.
function Application.RequestExit() end

-- ==============================================================================
-- ECS COMPONENTS
-- ==============================================================================

--- Property names (Renderer3D): Albedo (Vec3), Metallic, Roughness, Reflectance, Emission (Vec3), EmissionIntensity (floats),
--- UseAlbedoMap, UseNormalMap, UseMetallicMap, UseRoughnessMap, UseEmissionMap (ints)
---@class Material
local Material = {}
function Material:MarkDirty() end
---@param name string
---@param val number
function Material:SetFloat(name, val) end
---@param name string
---@return number
function Material:GetFloat(name) end
---@param name string
---@param val number
function Material:SetInt(name, val) end
---@param name string
---@return number
function Material:GetInt(name) end
---@param name string
---@param val Vec2
function Material:SetVec2(name, val) end
---@param name string
---@return Vec2
function Material:GetVec2(name) end
---@param name string
---@param val Vec3
function Material:SetVec3(name, val) end
---@param name string
---@return Vec3
function Material:GetVec3(name) end
---@param name string
---@param val Vec4
function Material:SetVec4(name, val) end
---@param name string
---@return Vec4
function Material:GetVec4(name) end

---@class NameComponent
---@field Name string
local NameComponent = {}

---@class TransformComponent
---@field Position Vec3
---@field Rotation Vec3
---@field Scale Vec3
---@field IsDirty boolean
local TransformComponent = {}
---@param offset Vec3
function TransformComponent:Translate(offset) end
---@param offset Vec3
function TransformComponent:Rotate(offset) end
---@param multi Vec3
function TransformComponent:ScaleBy(multi) end
function TransformComponent:MarkDirty() end

---@class SpriteRendererComponent
---@field Active boolean
---@field Color Vec4
local SpriteRendererComponent = {}

---@class CameraComponent
---@field Active boolean
---@field Primary boolean
---@field FixedAspectRatio boolean
local CameraComponent = {}

---@class MeshComponent
---@field Active boolean
---@field DropShadow boolean
local MeshComponent = {}
--- Loads and assigns a mesh asset from a project-relative path.
---@param meshAssetPath string
function MeshComponent:SetMesh(meshAssetPath) end
--- Gets the material at the given submesh index (from the base mesh asset).
---@param index number
---@return Material
function MeshComponent:GetMaterial(index) end
--- Overrides the material at the given submesh index. Accepts either a
--- project-relative path to a material asset, or an existing Material.
---@param index number
---@param materialPathOrMaterial string|Material
function MeshComponent:SetMaterialOverride(index, materialPathOrMaterial) end
---@param index number
function MeshComponent:ClearMaterialOverride(index) end
---@param index number
---@return boolean
function MeshComponent:HasOverride(index) end

---@class LightComponent
---@field Active boolean
---@field Color Vec3
---@field Intensity number
---@field Radius number
---@field Falloff number
---@field Type number
local LightComponent = {}

---@class EnvironmentComponent
---@field Active boolean
---@field Elevation number
---@field Azimuth number
---@field Turbidity number
---@field Exposure number
---@field SunIntensity number
---@field EnableSunDisk boolean
---@field SkyAmbient Vec3
---@field HorizonAmbient Vec3
---@field GroundAmbient Vec3
local EnvironmentComponent = {}

---@class ShadowSettings
---@field Enable boolean
---@field Quality number Read-only, ShadowQuality. Use ApplyQualityPreset() to change it.
---@field CascadeCount integer 1 - 3, every cascade draws its casters again
---@field Resolution integer Per cascade: 512, 1024, 2048 or 4096
---@field FilterSize integer Tent PCF width: 3, 5 or 7
---@field DitheredBlend boolean
---@field Distance number View distance the cascades cover
---@field FadeRange number Fraction of Distance over which shadows fade out
---@field CascadeSplitLambda number 0 = uniform splits, 1 = logarithmic
---@field CascadeBlend number
---@field ShowCascades boolean Debug tint, not saved
---@field NormalBias number
---@field DepthBiasConstant number
---@field DepthBiasClamp number
---@field DepthBiasSlope number
local ShadowSettings = {}
-- Sets Quality and the knobs it drives (cascades, resolution, filter, dithered blend)
---@param quality number ShadowQuality
function ShadowSettings:ApplyQualityPreset(quality) end

---@class AmbientOcclusionSettings
---@field Enable boolean
---@field Quality number Read-only, AOQuality. Use ApplyQualityPreset() to change it.
---@field ResolutionScale number 0.25 - 1, AO target size relative to the scene
---@field SliceCount integer
---@field StepsPerSlice integer
---@field BlurRadius integer 0 = no blur
---@field AccurateNormals boolean
---@field Radius number World space
---@field Intensity number
---@field FalloffRange number
---@field MaxDistance number
---@field BlurSharpness number
local AmbientOcclusionSettings = {}
-- Sets Quality and the knobs it drives (resolution, slices, steps, blur, normals)
---@param quality number AOQuality
function AmbientOcclusionSettings:ApplyQualityPreset(quality) end

---@class BloomSettings
---@field Enable boolean
---@field Quality number Read-only, BloomQuality. Use ApplyQualityPreset() to change it.
---@field UseCompute boolean
---@field MipCount integer 2 - 8
---@field HighQualityFilter boolean
---@field AntiFlicker boolean
---@field HighQualityComposite boolean
---@field Threshold number Linear HDR brightness where bloom starts
---@field Knee number
---@field Intensity number 0 skips bloom entirely
---@field Scatter number
---@field ClampValue number
---@field Tint Vec3
local BloomSettings = {}
-- Sets Quality and the knobs it drives (compute path, mip count, filters)
---@param quality number BloomQuality
function BloomSettings:ApplyQualityPreset(quality) end

---@class PostProcessSettings
---@field VignetteIntensity number
---@field VignetteSoftness number
---@field Grain number
---@field ShowAOOnly boolean Debug view, not saved
local PostProcessSettings = {}

-- One profile of a RendererSettingsComponent. Nested settings are references: profile.Bloom.Intensity = 2 edits the component,
-- the Renderer applies the change the next frame
---@class RenderSettings
---@field RenderScale number 0.5 - 1, resolution of the 3D scene (UI and post-processing stay at full resolution)
---@field MaxLights integer
---@field MaxParticles integer
---@field EmissionQuality number 0.1 - 1
---@field MaxSkinMatrices integer
---@field Shadows ShadowSettings
---@field AO AmbientOcclusionSettings
---@field Bloom BloomSettings
---@field PostProcess PostProcessSettings
local RenderSettings = {}

-- Render settings of the scene, one profile per platform (Android: Mobile, Windows: Desktop). The first active one in the scene is used
---@class RendererSettingsComponent
---@field Active boolean
---@field Desktop RenderSettings
---@field Mobile RenderSettings
local RendererSettingsComponent = {}
-- The profile this platform renders with (in the Editor: the one being previewed). Use it for in-game graphics options
---@return RenderSettings
function RendererSettingsComponent:GetCurrentProfile() end
-- Puts a profile back to the engine defaults of that platform
---@param profile number RenderProfile
function RendererSettingsComponent:ResetProfile(profile) end

---@class RigidbodyComponent
---@field Active boolean
---@field Type number
---@field Layer integer Physics layer (0-15, Physics.GetLayer), takes effect immediately
---@field Mass number
---@field Interpolate boolean
---@field UseGravity boolean
---@field IsSensor boolean
---@field ContinuousCollision boolean
---@field FreezeRotationX boolean
---@field FreezeRotationY boolean
---@field FreezeRotationZ boolean
---@field LinearDamping number
---@field AngularDamping number
---@field Friction number
---@field Bounciness number
local RigidbodyComponent = {}
---@param force Vec3
function RigidbodyComponent:AddForce(force) end
---@param impulse Vec3
function RigidbodyComponent:AddImpulse(impulse) end
---@param vel Vec3
function RigidbodyComponent:SetLinearVelocity(vel) end
---@return Vec3
function RigidbodyComponent:GetLinearVelocity() end
---@param torque Vec3
function RigidbodyComponent:AddTorque(torque) end
---@param impulse Vec3
function RigidbodyComponent:AddAngularImpulse(impulse) end
---@param vel Vec3
function RigidbodyComponent:SetAngularVelocity(vel) end
---@return Vec3
function RigidbodyComponent:GetAngularVelocity() end

---@class BoxColliderComponent
---@field Active boolean
---@field ShowCollider boolean
---@field HalfExtents Vec3
local BoxColliderComponent = {}

---@class SphereColliderComponent
---@field Active boolean
---@field ShowCollider boolean
---@field Radius number
local SphereColliderComponent = {}

---@class CapsuleColliderComponent
---@field Active boolean
---@field ShowCollider boolean
---@field Height number
---@field Radius number
local CapsuleColliderComponent = {}

---@class CylinderColliderComponent
---@field Active boolean
---@field ShowCollider boolean
---@field Height number
---@field Radius number
local CylinderColliderComponent = {}

---@class ConvexColliderComponent
---@field Active boolean
---@field ShowCollider boolean
---@field LocalOffset Vec3
---@field LocalRotation Vec3
local ConvexColliderComponent = {}

---@class MeshColliderComponent
---@field Active boolean
---@field LocalOffset Vec3
---@field LocalRotation Vec3
local MeshColliderComponent = {}

---@class ScriptComponent
---@field Active boolean Turns every script of the entity off (OnDisable) and on (OnEnable)
---@field Count integer Number of scripts on the entity
local ScriptComponent = {}
--- Adds a script asset (project-relative path). It starts at the beginning of the next frame (OnCreate, OnEnable).
---@param scriptAssetPath string
---@return boolean
function ScriptComponent:AddScript(scriptAssetPath) end
--- Same as AddScript (kept for older scripts).
---@param scriptAssetPath string
---@return boolean
function ScriptComponent:SetScript(scriptAssetPath) end
--- name: the script's file name without extension, e.g. "Player" for Scripts/Player.lua
---@param name string
---@return boolean
function ScriptComponent:HasScript(name) end
---@param name string
---@return boolean
function ScriptComponent:IsScriptActive(name) end
--- OnDisable / OnEnable run at the start of the next frame.
---@param name string
---@param active boolean
---@return boolean
function ScriptComponent:SetScriptActive(name, active) end

---@class TextComponent
---@field Active boolean
---@field Text string
---@field Color Vec4
---@field MaxWidth number
---@field LetterSpacing number
---@field Alignment number
---@field VerticalAlignment number
---@field LineSpacing number
---@field ShadowEnabled boolean
---@field ShadowColor Vec4
---@field ShadowOffset Vec2
local TextComponent = {}

---@class UICanvasComponent
---@field Active boolean
---@field ShowCanvas boolean
---@field SortOrder integer Higher draws on top and receives input first
---@field ScriptAsset number
local UICanvasComponent = {}

--- Finds a widget of this canvas by name (UI Layout made in the UI Editor, or the SetUIRoot tree), returns the concrete type or nil.
--- Example: entity.UICanvasC:FindWidget("PlayButton"):OnClick(function(self) Log(self.Name) end)
---@param name string
---@return UIWidget|UIButton|UIText|UIImage|UIImageButton|UIProgressBar|UIToggle|UISlider|UIJoystick|UIScrollView|UIListView|UILayoutGroup|UIInputField|UIDropdown|UIModal|UITabView|UISpinner|UISafeArea|nil
function UICanvasComponent:FindWidget(name) end

--- Topmost interactable widget of this canvas at a window position (e.g. a touch position), or nil.
---@param x number
---@param y number
---@return UIWidget|UIButton|UIText|UIImage|UIImageButton|UIProgressBar|UIToggle|UISlider|UIJoystick|UIScrollView|UIListView|UILayoutGroup|UIInputField|UIDropdown|UIModal|UITabView|UISpinner|UISafeArea|nil
function UICanvasComponent:WidgetAt(x, y) end

--- Window position -> canvas units (the units widget Offset/Size are authored in, (0,0) = top left).
---@param x number
---@param y number
---@return Vec2
function UICanvasComponent:ScreenToCanvas(x, y) end

--- Visible canvas size in canvas units (the height matches the reference resolution when matching height).
---@return Vec2
function UICanvasComponent:GetCanvasSize() end

---@class AudioSourceComponent
---@field Active boolean
---@field Volume number
---@field Pitch number
---@field Loop boolean
---@field PlayOnAwake boolean
---@field IsSpatialized boolean
---@field IsStreaming boolean
---@field IsPlaying boolean Read-only.
---@field IsInitialized boolean Read-only.
---@field MinDistance number
---@field MaxDistance number
---@field Attenuation number AttenuationModel enum value.
local AudioSourceComponent = {}
function AudioSourceComponent:Play() end
function AudioSourceComponent:Stop() end
--- Seeks the audio to given time (seconds)
---@param time number
function AudioSourceComponent:Seek(time) end
--- Loads and assigns an audio clip from a project-relative path.
---@param audioAssetPath string
function AudioSourceComponent:SetAudioClip(audioAssetPath) end

--- Plays the animation clips of the entity's mesh (a glTF with a skin or animated nodes). Needs a MeshC on the same entity.
--- Clips come from the mesh itself and from up to 4 clip sources set in the Inspector (other animated meshes with the same joint names).
---   local anim = Self.AnimatorC
---   anim:Play("Run", 0.2) -- crossfade over 0.2 s
---@class AnimatorComponent
---@field Active boolean Inactive animators show the rest pose.
---@field Clip string Clip played when the scene starts (PlayOnAwake), empty = the rest pose until Play().
---@field PlayOnAwake boolean
---@field Loop boolean Applies to the playing clips.
---@field Speed number Playback speed, negative plays backwards.
---@field AnimateOffscreen boolean false: outside the camera only the time advances (saves CPU on phones).
---@field Paused boolean
---@field CurrentClip string Read-only. Name of the playing clip, empty when none.
local AnimatorComponent = {}
--- Plays a clip by name, crossfading from the current one over fadeSeconds (0 or nil = instant). Playing the clip that already
--- plays does nothing. Returns false when the animator has no clip with that name.
---@param clip string
---@param fadeSeconds number|nil
---@return boolean
function AnimatorComponent:Play(clip, fadeSeconds) end
--- Stops every clip, back to the rest pose.
function AnimatorComponent:Stop() end
--- True while a clip plays (a non looping clip stops at its end). With a name: true only if that clip plays.
---@param clip string|nil
---@return boolean
function AnimatorComponent:IsPlaying(clip) end
---@param clip string
---@return boolean
function AnimatorComponent:HasClip(clip) end
--- Seconds into the playing clip.
---@return number
function AnimatorComponent:GetTime() end
---@param seconds number
function AnimatorComponent:SetTime(seconds) end
--- Length in seconds of a clip (the playing one without a name), 0 if unknown.
---@param clip string|nil
---@return number
function AnimatorComponent:GetDuration(clip) end
---@return string[]
function AnimatorComponent:GetClipNames() end
--- Index of a joint (skeleton node) by name, -1 when not found. Available once the animator updated (from the first OnUpdate on),
--- look it up once and keep the index:
---   if hand == nil or hand < 0 then hand = anim:FindJoint("mixamorig:RightHand") end
---@param name string
---@return integer
function AnimatorComponent:FindJoint(name) end
--- World position and rotation (degrees) of a joint in this frame's pose, nil before the first pose. joint: an index from FindJoint or a name.
---   local position, rotation = anim:GetJointWorldTransform(hand)
---@param joint integer|string
---@return Vec3|nil position
---@return Vec3|nil rotation
function AnimatorComponent:GetJointWorldTransform(joint) end

--- Plays a particle effect asset (.spfx, made in the Editor's Particle Editor).
---@class ParticleSystemComponent
---@field Active boolean Inactive systems are neither simulated nor drawn.
---@field AutoPlay boolean Starts on its own when the scene runs.
---@field AutoDestroy boolean Destroys the entity once the effect finished (one-shot effects).
---@field SimulationSpeed number
---@field EmissionMultiplier number Scales every emission rate and burst.
---@field Seed integer 0 = a different random sequence every play.
---@field Culling number ParticleCullingMode enum value.
---@field IsPlaying boolean Read-only.
---@field IsPaused boolean Read-only.
---@field IsAlive boolean Read-only. Emitting or particles left.
---@field ParticleCount integer Read-only.
local ParticleSystemComponent = {}
--- Starts the effect, or resumes it when paused.
function ParticleSystemComponent:Play() end
--- Stops emitting, the live particles finish unless clear is true.
---@param clear boolean|nil
function ParticleSystemComponent:Stop(clear) end
function ParticleSystemComponent:Pause() end
--- Clears the particles and plays from the start.
function ParticleSystemComponent:Restart() end
--- Removes every live particle.
function ParticleSystemComponent:Clear() end
--- Spawns particles right away in every emitter (on the next update), works without Play().
---@param count integer
function ParticleSystemComponent:Emit(count) end
--- Loads and assigns a particle effect from an Assets-relative path (e.g. "Effects/Fire.spfx").
---@param effectAssetPath string
function ParticleSystemComponent:SetEffect(effectAssetPath) end

--- Fire-and-forget particle effects in the running scene.
---@class Particles
Particles = {}
--- Plays an effect once at a position: creates an entity with an AutoDestroy ParticleSystemC, removed when the effect finished.
--- Returns the entity (nil if the path is not a particle effect). Looping effects keep running until you stop or destroy them.
---   Particles.Play("Effects/Hit.spfx", hit.Point)
---@param effectAssetPath string
---@param position Vec3
---@param rotation Vec3|nil Degrees
---@return Entity|nil
function Particles.Play(effectAssetPath, position, rotation) end

-- ==============================================================================
-- ECS ENTITY
-- ==============================================================================

---@class Entity
---@field NameC NameComponent|nil
---@field TransformC TransformComponent|nil
---@field SpriteRendererC SpriteRendererComponent|nil
---@field CameraC CameraComponent|nil
---@field MeshC MeshComponent|nil
---@field LightC LightComponent|nil
---@field EnvironmentC EnvironmentComponent|nil
---@field RendererSettingsC RendererSettingsComponent|nil
---@field RigidbodyC RigidbodyComponent|nil
---@field BoxColliderC BoxColliderComponent|nil
---@field SphereColliderC SphereColliderComponent|nil
---@field CapsuleColliderC CapsuleColliderComponent|nil
---@field CylinderColliderC CylinderColliderComponent|nil
---@field ConvexColliderC ConvexColliderComponent|nil
---@field MeshColliderC MeshColliderComponent|nil
---@field ScriptC ScriptComponent|nil
---@field TextC TextComponent|nil
---@field UICanvasC UICanvasComponent|nil
---@field AudioSourceC AudioSourceComponent|nil
---@field ParticleSystemC ParticleSystemComponent|nil
---@field AnimatorC AnimatorComponent|nil
local Entity = {}

---@return boolean
function Entity:IsValid() end

---@return number
function Entity:GetID() end

--- Marks the entity for deferred destruction (end of frame).
function Entity:Destroy() end

--- Destroys the entity immediately.
function Entity:DestroyImmediate() end

---@param targetName string
---@return Entity
function Entity:FindEntityByName(targetName) end

--- Creates a new child entity in this entity's scene.
---@param name string
---@return Entity
function Entity:CreateEntity(name) end

--- Reparents this entity under the given parent entity.
---@param parent Entity
function Entity:SetParent(parent) end

--- The environment of one of this entity's scripts: its global variables, functions and Properties.
--- With a name (the script's file name, e.g. "Health") that script, without one the first script
--- (or the UI canvas script). nil until the script is instantiated; every script that starts in the
--- same frame is instantiated before any OnCreate runs, so this works in OnCreate.
---   local health = entity:FindEntityByName("Enemy"):GetScript("Health")
---   health.TakeDamage(10); print(health.Properties.Max)
---@param name string|nil
---@return table|nil
function Entity:GetScript(name) end

--- Every instantiated script environment of this entity, in Script Component order.
---@return table[]
function Entity:GetScripts() end

-- Generated Has/Add/Remove/GetOrAdd functions
---@return boolean
function Entity:HasNameC() end
---@return NameComponent
function Entity:AddNameC() end
function Entity:RemoveNameC() end

---@return boolean
function Entity:HasTransformC() end
---@return TransformComponent
function Entity:AddTransformC() end
function Entity:RemoveTransformC() end

---@return boolean
function Entity:HasSpriteRendererC() end
---@return SpriteRendererComponent
function Entity:AddSpriteRendererC() end
function Entity:RemoveSpriteRendererC() end

---@return boolean
function Entity:HasCameraC() end
---@return CameraComponent
function Entity:AddCameraC() end
function Entity:RemoveCameraC() end

---@return boolean
function Entity:HasMeshC() end
---@return MeshComponent
function Entity:AddMeshC() end
function Entity:RemoveMeshC() end

---@return boolean
function Entity:HasLightC() end
---@return LightComponent
function Entity:AddLightC() end
function Entity:RemoveLightC() end

---@return boolean
function Entity:HasEnvironmentC() end
---@return EnvironmentComponent
function Entity:AddEnvironmentC() end
function Entity:RemoveEnvironmentC() end

---@return boolean
function Entity:HasRendererSettingsC() end
---@return RendererSettingsComponent
function Entity:AddRendererSettingsC() end
function Entity:RemoveRendererSettingsC() end

---@return boolean
function Entity:HasRigidbodyC() end
---@return RigidbodyComponent
function Entity:AddRigidbodyC() end
function Entity:RemoveRigidbodyC() end

---@return boolean
function Entity:HasBoxColliderC() end
---@return BoxColliderComponent
function Entity:AddBoxColliderC() end
function Entity:RemoveBoxColliderC() end

---@return boolean
function Entity:HasSphereColliderC() end
---@return SphereColliderComponent
function Entity:AddSphereColliderC() end
function Entity:RemoveSphereColliderC() end

---@return boolean
function Entity:HasCapsuleColliderC() end
---@return CapsuleColliderComponent
function Entity:AddCapsuleColliderC() end
function Entity:RemoveCapsuleColliderC() end

---@return boolean
function Entity:HasCylinderColliderC() end
---@return CylinderColliderComponent
function Entity:AddCylinderColliderC() end
function Entity:RemoveCylinderColliderC() end

---@return boolean
function Entity:HasConvexColliderC() end
---@return ConvexColliderComponent
function Entity:AddConvexColliderC() end
function Entity:RemoveConvexColliderC() end

---@return boolean
function Entity:HasMeshColliderC() end
---@return MeshColliderComponent
function Entity:AddMeshColliderC() end
function Entity:RemoveMeshColliderC() end

---@return boolean
function Entity:HasScriptC() end
---@return ScriptComponent
function Entity:AddScriptC() end
function Entity:RemoveScriptC() end

---@return boolean
function Entity:HasTextC() end
---@return TextComponent
function Entity:AddTextC() end
function Entity:RemoveTextC() end

---@return boolean
function Entity:HasUICanvasC() end
---@return UICanvasComponent
function Entity:AddUICanvasC() end
function Entity:RemoveUICanvasC() end

---@return boolean
function Entity:HasAudioSourceC() end
---@return AudioSourceComponent
function Entity:AddAudioSourceC() end
function Entity:RemoveAudioSourceC() end

---@return boolean
function Entity:HasParticleSystemC() end
---@return ParticleSystemComponent
function Entity:AddParticleSystemC() end
function Entity:RemoveParticleSystemC() end

---@return boolean
function Entity:HasAnimatorC() end
---@return AnimatorComponent
function Entity:AddAnimatorC() end
function Entity:RemoveAnimatorC() end