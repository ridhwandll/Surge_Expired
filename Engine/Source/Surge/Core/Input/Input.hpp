// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/Input/KeyCodes.hpp"
#include "Surge/Core/Input/MouseCodes.hpp"

namespace Surge
{
    enum class CursorMode
    {
        Normal = 0,
        Locked
    };

    class Input
    {
    public:
        static bool IsKeyPressed(KeyCode key);
        static bool IsMouseButtonPressed(MouseCode button);

        static Pair<float, float> GetMousePosition();
        static float GetMouseX();
        static float GetMouseY();
        static void SetCursorMode(CursorMode cursorMode);
    };

} // namespace Surge
