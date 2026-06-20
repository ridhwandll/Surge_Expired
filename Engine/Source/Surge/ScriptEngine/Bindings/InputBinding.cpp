// Copyright (c) - SurgeTechnologies - All rights reserved
#include "InputBinding.hpp"
#include "Surge/Core/Input/Input.hpp"
#include "Surge/ScriptEngine/Lua.hpp"
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
        inputTable["SetCursorMode"] = &Input::SetCursorMode;
        inputTable["GetMousePosition"] = []() -> glm::vec2 { auto pos = Input::GetMousePosition(); return glm::vec2(pos.Data1, pos.Data2);};
    }
}

