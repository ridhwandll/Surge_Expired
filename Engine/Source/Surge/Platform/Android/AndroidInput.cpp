// Copyright (c) - SurgeTechnologies - All rights reserved
#ifdef SURGE_PLATFORM_ANDROID
#include "Surge/Core/Input/Input.hpp"
#include "Surge/Platform/Android/AndroidWindow.hpp"

namespace Surge
{
    // Android has no keyboard; key queries always return false
    bool Input::IsKeyPressed(Key)
    {
        //Log<Severity::Warn>("Android has no keyboard so bool Input::IsKeyPressed(Key) is always return false!");
        return false;
    }

    // Primary touch maps to left mouse button
    bool Input::IsMouseButtonPressed(Mouse button)
    {
        if (button == Mouse::ButtonLeft)
            return Android::IsTouchDown();

        return false;
    }

    Pair<float, float> Input::GetMousePosition()
    {
        return {Android::GetTouchX(), Android::GetTouchY()};
    }

    float Input::GetMouseX() { return Android::GetTouchX(); }

    float Input::GetMouseY() { return Android::GetTouchY(); }

    void Input::SetCursorMode(CursorMode)
    {
        // No cursor on Android
        //Log<Severity::Warn>("Android has no mouse, you cant do Input::SetCursorMode(CursorMode) on your finger!");
    }

} // namespace Surge

#endif // SURGE_PLATFORM_ANDROID