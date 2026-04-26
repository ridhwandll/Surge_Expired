// Copyright (c) - SurgeTechnologies - All rights reserved
#ifdef SURGE_PLATFORM_ANDROID
#include "Surge/Core/Input/Input.hpp"
#include "Surge/Platform/Android/AndroidWindow.hpp"

namespace Surge
{
    // Android has no keyboard; key queries always return false.
    bool Input::IsKeyPressed(KeyCode)
    {
        return false;
    }

    // Primary touch maps to left mouse button.
    bool Input::IsMouseButtonPressed(const MouseCode button)
    {
        if (button == Mouse::ButtonLeft)
            return AndroidIsTouchDown();
        return false;
    }

    Pair<float, float> Input::GetMousePosition()
    {
        return {AndroidGetTouchX(), AndroidGetTouchY()};
    }

    float Input::GetMouseX() { return AndroidGetTouchX(); }

    float Input::GetMouseY() { return AndroidGetTouchY(); }

    void Input::SetCursorMode(CursorMode)
    {
        // No cursor on Android
    }

} // namespace Surge

#endif // SURGE_PLATFORM_ANDROID