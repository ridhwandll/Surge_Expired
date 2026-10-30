// Copyright (c) - SurgeTechnologies - All rights reserved
#include "InputBinding.hpp"
#include "Surge/Core/Input/Input.hpp"
#include "Surge/ScriptEngine/Lua.hpp"
#include "BindingUtils.hpp"
#include <glm/vec2.hpp>

namespace Surge::ScriptBinding
{
    void BindInput(void* luaState)
    {
        sol::state_view& lua = *static_cast<sol::state_view*>(luaState);

        lua.new_enum("Mouse",
                     "ButtonLeft", Mouse::ButtonLeft,
                     "ButtonRight", Mouse::ButtonRight,
                     "ButtonMiddle", Mouse::ButtonMiddle
        );

        lua.new_enum("Key",
                     "Space", Key::Space,
                     "Comma", Key::Comma,
                     "Minus", Key::Minus,
                     "Period", Key::Period,
                     "D0", Key::D0, "D1", Key::D1, "D2", Key::D2, "D3", Key::D3, "D4", Key::D4,
                     "D5", Key::D5, "D6", Key::D6, "D7", Key::D7, "D8", Key::D8, "D9", Key::D9,
                     "Semicolon", Key::Semicolon,
                     "A", Key::A, "B", Key::B, "C", Key::C, "D", Key::D, "E", Key::E, "F", Key::F,
                     "G", Key::G, "H", Key::H, "I", Key::I, "J", Key::J, "K", Key::K, "L", Key::L,
                     "M", Key::M, "N", Key::N, "O", Key::O, "P", Key::P, "Q", Key::Q, "R", Key::R,
                     "S", Key::S, "T", Key::T, "U", Key::U, "V", Key::V, "W", Key::W, "X", Key::X,
                     "Y", Key::Y, "Z", Key::Z,
                     "LeftBracket", Key::LeftBracket, "BackSlash", Key::BackSlash, "RightBracket", Key::RightBracket,
                     "GraveAccent", Key::GraveAccent,
                     "Backspace", Key::Backspace, "Enter", Key::Enter, "Tab", Key::Tab,
                     "Pause", Key::Pause, "NumLock", Key::NumLock, "ScrollLock", Key::ScrollLock,
                     "CapsLock", Key::CapsLock, "Escape", Key::Escape,
                     "PageUp", Key::PageUp, "PageDown", Key::PageDown, "End", Key::End, "Home", Key::Home,
                     "Left", Key::Left, "Up", Key::Up, "Right", Key::Right, "Down", Key::Down,
                     "PrintScreen", Key::PrintScreen, "Insert", Key::Insert, "Delete", Key::Delete,
                     "F1", Key::F1, "F2", Key::F2, "F3", Key::F3, "F4", Key::F4, "F5", Key::F5,
                     "F6", Key::F6, "F7", Key::F7, "F8", Key::F8, "F9", Key::F9, "F10", Key::F10,
                     "F11", Key::F11, "F12", Key::F12, "F13", Key::F13, "F14", Key::F14, "F15", Key::F15,
                     "F16", Key::F16, "F17", Key::F17, "F18", Key::F18, "F19", Key::F19, "F20", Key::F20,
                     "F21", Key::F21, "F22", Key::F22, "F23", Key::F23, "F24", Key::F24,
                     "KP0", Key::KP0, "KP1", Key::KP1, "KP2", Key::KP2, "KP3", Key::KP3, "KP4", Key::KP4,
                     "KP5", Key::KP5, "KP6", Key::KP6, "KP7", Key::KP7, "KP8", Key::KP8, "KP9", Key::KP9,
                     "KPMultiply", Key::KPMultiply, "KPAdd", Key::KPAdd, "KPEqual", Key::KPEqual,
                     "KPSubtract", Key::KPSubtract, "KPDecimal", Key::KPDecimal, "KPDivide", Key::KPDivide,
                     "LeftShift", Key::LeftShift, "RightShift", Key::RightShift,
                     "LeftControl", Key::LeftControl, "RightControl", Key::RightControl,
                     "LeftAlt", Key::LeftAlt, "RightAlt", Key::RightAlt
        );

        sol::table inputTable = lua.create_named_table("Input");
        inputTable["IsKeyPressed"] = &Input::IsKeyPressed;
        inputTable["IsMouseButtonPressed"] = &Input::IsMouseButtonPressed;
        inputTable["GetMouseX"] = &Input::GetMouseX;
        inputTable["GetMouseY"] = &Input::GetMouseY;
        lua.new_enum("CursorMode",
                     "Normal", CursorMode::Normal,
                     "Locked", CursorMode::Locked
        );
        inputTable["SetCursorMode"] = &Input::SetCursorMode;
        inputTable["GetMousePosition"] = []() -> glm::vec2 { auto pos = Input::GetMousePosition(); return glm::vec2(pos.Data1, pos.Data2);};
        inputTable["SetMousePosition"] = &Input::SetMousePosition; // Mouse look: re-center the locked cursor every frame
        inputTable["WasKeyPressed"] = &Input::WasKeyPressed;
        inputTable["WasKeyReleased"] = &Input::WasKeyReleased;
        inputTable["WasMouseButtonPressed"] = &Input::WasMouseButtonPressed;
        inputTable["WasMouseButtonReleased"] = &Input::WasMouseButtonReleased;
        inputTable["GetMouseScrollDelta"] = &Input::GetMouseScrollDelta;

        // Touch: Lua indices are 1-based (Input.GetTouch(1) .. Input.GetTouch(Input.GetTouchCount())), touches are copied
        lua.new_enum("TouchPhase",
                     "BEGAN", TouchPhase::BEGAN,
                     "MOVED", TouchPhase::MOVED,
                     "STATIONARY", TouchPhase::STATIONARY,
                     "ENDED", TouchPhase::ENDED,
                     "CANCELED", TouchPhase::CANCELED
        );
        lua.new_usertype<Touch>("Touch", sol::no_constructor,
                                "ID", sol::readonly_property([](const Touch& t) { return t.ID; }),
                                "Phase", sol::readonly_property([](const Touch& t) { return t.Phase; }),
                                "Position", sol::readonly_property([](const Touch& t) { return t.Position; }),
                                "StartPosition", sol::readonly_property([](const Touch& t) { return t.StartPosition; }),
                                "Delta", sol::readonly_property([](const Touch& t) { return t.Delta; }),
                                "X", sol::readonly_property([](const Touch& t) { return t.Position.x; }),
                                "Y", sol::readonly_property([](const Touch& t) { return t.Position.y; }),
                                "Pressure", sol::readonly_property([](const Touch& t) { return t.Pressure; }),
                                "Duration", sol::readonly_property([](const Touch& t) { return t.Duration; }),
                                "IsMouse", sol::readonly_property([](const Touch& t) { return t.IsMouse; }),
                                STRICT_READ(Touch)
        );
        inputTable["GetTouchCount"] = &Input::GetTouchCount;
        inputTable["GetTouch"] = [](int index, sol::this_state s) -> sol::object {
            if(index < 1 || static_cast<Uint>(index) > Input::GetTouchCount())
                return sol::make_object(s, sol::lua_nil);
            return sol::make_object(s, Input::GetTouch(static_cast<Uint>(index - 1)));
        };
        inputTable["GetTouchByID"] = [](int32_t id, sol::this_state s) -> sol::object {
            const Touch* touch = Input::FindTouch(id);
            return touch ? sol::make_object(s, *touch) : sol::make_object(s, sol::lua_nil);
        };
        inputTable["GetTouches"] = [](sol::this_state s) -> sol::table {
            sol::state_view lua(s);
            const Uint count = Input::GetTouchCount();
            sol::table touches = lua.create_table(static_cast<int>(count), 0);
            for(Uint i = 0; i < count; i++)
                touches[i + 1] = Input::GetTouch(i);
            return touches;
        };
        inputTable["IsTouchSupported"] = &Input::IsTouchSupported;
        inputTable["SetMouseTouchEmulation"] = &Input::SetMouseTouchEmulation;
        inputTable["IsMouseTouchEmulationEnabled"] = &Input::IsMouseTouchEmulationEnabled;

        // Gamepads: Lua indices are 1-based (1 .. Input.MaxGamepads)
        lua.new_enum("GamepadButton",
                     "A", GamepadButton::A, "B", GamepadButton::B, "X", GamepadButton::X, "Y", GamepadButton::Y,
                     "LeftShoulder", GamepadButton::LeftShoulder, "RightShoulder", GamepadButton::RightShoulder,
                     "Back", GamepadButton::Back, "Start", GamepadButton::Start, "Guide", GamepadButton::Guide,
                     "LeftThumb", GamepadButton::LeftThumb, "RightThumb", GamepadButton::RightThumb,
                     "DPadUp", GamepadButton::DPadUp, "DPadDown", GamepadButton::DPadDown,
                     "DPadLeft", GamepadButton::DPadLeft, "DPadRight", GamepadButton::DPadRight
        );
        lua.new_enum("GamepadAxis",
                     "LeftX", GamepadAxis::LeftX, "LeftY", GamepadAxis::LeftY,
                     "RightX", GamepadAxis::RightX, "RightY", GamepadAxis::RightY,
                     "LeftTrigger", GamepadAxis::LeftTrigger, "RightTrigger", GamepadAxis::RightTrigger
        );
        inputTable["MaxGamepads"] = Input::MAX_GAMEPADS;
        auto padIndex = [](int index) -> Uint { return index >= 1 ? static_cast<Uint>(index - 1) : Input::MAX_GAMEPADS; };
        inputTable["IsGamepadConnected"] = [padIndex](int index) { return Input::IsGamepadConnected(padIndex(index)); };
        inputTable["IsGamepadButtonPressed"] = [padIndex](int index, GamepadButton b) { return Input::IsGamepadButtonPressed(padIndex(index), b); };
        inputTable["WasGamepadButtonPressed"] = [padIndex](int index, GamepadButton b) { return Input::WasGamepadButtonPressed(padIndex(index), b); };
        inputTable["WasGamepadButtonReleased"] = [padIndex](int index, GamepadButton b) { return Input::WasGamepadButtonReleased(padIndex(index), b); };
        inputTable["GetGamepadAxis"] = [padIndex](int index, GamepadAxis a) { return Input::GetGamepadAxis(padIndex(index), a); };

        // Android back button (reported as Key.Escape): false = the game handles it, the app no longer closes
        inputTable["SetBackButtonExitsApp"] = &Input::SetBackButtonExitsApp;
        inputTable["DoesBackButtonExitApp"] = &Input::DoesBackButtonExitApp;
    }
}

