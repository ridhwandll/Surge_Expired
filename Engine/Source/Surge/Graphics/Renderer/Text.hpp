// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include <cstdint>

namespace Surge
{
    enum class TextAlignment : uint8_t
    {
        LEFT = 0,
        CENTER,
        RIGHT
    };

    enum class TextVerticalAlignment : uint8_t
    {
        TOP = 0,
        CENTER,
        BASELINE,
        BOTTOM
    };
}
