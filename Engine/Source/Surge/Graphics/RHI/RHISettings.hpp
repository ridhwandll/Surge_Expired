#pragma once
#include "Surge/Core/Defines.hpp"

// Settings file for RHI, no instantiation of struct, just modify here as-is

namespace Surge
{	
	struct RHISettings
	{
		inline static constexpr Uint FRAMES_IN_FLIGHT = 3;
		inline static constexpr Uint MAX_BINDLESS_TEXTURES = 4096;
	};
}