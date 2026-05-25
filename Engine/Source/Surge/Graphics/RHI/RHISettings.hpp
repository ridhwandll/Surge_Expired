#pragma once
#include "Surge/Core/Defines.hpp"

// Settings file for RHI, no instantiation of struct, just modify here as-is

namespace Surge
{	
    struct RHISettings
    {
        inline static bool RENDER_TO_SWAPCHAIN = false; // For Runtime/Player we do RENDER_TO_SWAPCHAIN = true, else RENDER_TO_SWAPCHAIN = false for Editor
        inline static constexpr Uint FRAMES_IN_FLIGHT = 2; // (RID) We default to 2 because we are a mobile engine 8)
        inline static constexpr Uint MAX_BINDLESS_TEXTURES = 4096;
    };
}