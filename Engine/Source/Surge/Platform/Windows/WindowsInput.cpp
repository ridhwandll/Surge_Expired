// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Core/Input/Input.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Core/Window/Window.hpp"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

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
        static CursorMode sLastCursorMode = CursorMode::Normal;
        if(sLastCursorMode == cursorMode)
            return;

        HWND hWnd = (HWND)Core::GetWindow()->GetNativeWindowHandle();

        if(cursorMode == CursorMode::Normal)
        {
            ClipCursor(NULL);
            ShowCursor(TRUE);
        }
        else if(cursorMode == CursorMode::Locked)
        {
            RECT rect;
            GetClientRect(hWnd, &rect);

            POINT ul = { rect.left, rect.top };
            POINT lr = { rect.right, rect.bottom };
            ClientToScreen(hWnd, &ul);
            ClientToScreen(hWnd, &lr);

            RECT screenRect = { ul.x, ul.y, lr.x, lr.y };

            ClipCursor(&screenRect);
            ShowCursor(FALSE);
        }

        sLastCursorMode = cursorMode;
    }
} // namespace Surge
