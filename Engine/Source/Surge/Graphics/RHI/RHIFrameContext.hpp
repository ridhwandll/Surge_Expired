// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"

namespace Surge
{
	struct FrameContext
	{
		Uint FrameIndex = 0;
		Uint SwapchainIndex = 0;
		Uint Width = 0;
		Uint Height = 0;
	};

} // namespace Surge