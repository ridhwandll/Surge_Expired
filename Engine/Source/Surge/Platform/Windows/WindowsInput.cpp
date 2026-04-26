// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Core/Input/Input.hpp"
#include "Surge/Core/Core.hpp"

namespace Surge
{
    bool Input::IsKeyPressed(KeyCode key)
    {
        if (GetActiveWindow() == Surge::Core::GetWindow()->GetNativeWindowHandle())
        {
            SHORT state = GetAsyncKeyState(static_cast<int>(key));
            return (state & 0x8000);
        }
        return false;
    }

    bool Input::IsMouseButtonPressed(const MouseCode button)
    {
        if (GetActiveWindow() == Surge::Core::GetWindow()->GetNativeWindowHandle())
        {
            SHORT state = GetAsyncKeyState(static_cast<int>(button));
            return (state & 0x8000);
        }
        return false;
    }

    Pair<float, float> Input::GetMousePosition()
    {
        POINT p;
        GetCursorPos(&p);
        return {(float)p.x, (float)p.y};
    }

    float Input::GetMouseX() { return GetMousePosition().Data1; }

    float Input::GetMouseY() { return GetMousePosition().Data2; }

    void Input::SetCursorMode(CursorMode cursorMode)
    {
        switch (cursorMode)
        {
            case CursorMode::Normal: SetCursor(LoadCursor(nullptr, IDC_ARROW)); break;
            case CursorMode::Locked: SetCursor(nullptr); break;
        }
    }
} // namespace Surge
