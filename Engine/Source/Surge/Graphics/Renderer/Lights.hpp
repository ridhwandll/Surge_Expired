// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include <glm/glm.hpp>

namespace Surge
{
	struct GPULight
	{
		glm::vec4 PositionType;				  // 16 bytes -> xyz = pos or dir, w = type (0 = dir, 1 = point)
		glm::vec4 ColorRGBIntensityA;         // 16 bytes -> rgb=color, a=intensity
		float Radius;						  // 4 bytes  -> point light falloff
		float Falloff;						  // 4 bytes  -> point light falloff curve

		glm::vec2 _pad0;
	};
	static_assert(sizeof(GPULight) % 16 == 0, "Size of 'GPULight' struct must be 16 bytes aligned!");

	enum class LightType : Uint
	{
		POINT = 0,
		DIRECTIONAL = 1
	};
	
} // namespace Surge
