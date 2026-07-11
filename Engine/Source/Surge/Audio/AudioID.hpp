// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include <cstdint>

namespace Surge
{
    using AudioID = void*;

    enum class AttenuationModel : uint8_t
    {
        NONE = 0,
        INVERSE_DISTANCE = 1,
        LINEAR_DISTANCE = 2,
        EXPONENTIAL_DISTANCE = 3
    };

}
