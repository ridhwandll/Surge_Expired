// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Window/Window.hpp"

#if defined(SURGE_PLATFORM_WINDOWS) || defined(SURGE_PLATFORM_ANDROID)
#include "Vulkan/VulkanRHI.hpp"
namespace Surge
{
	using BackendRHI = VulkanRHI;
}
#elif defined(SURGE_PLATFORM_APPLE)
#include "Metal/MetalRHI.hpp"
namespace Surge
{
	using BackendRHI = MetalRHI;
}
#endif


namespace Surge
{
	class GraphicsRHI
	{
	public:
		GraphicsRHI() = default;
		~GraphicsRHI() = default;

		void Initialize(Window* window = nullptr)
		{
			mBackendRHI.Initialize(window);
		}

		void Shutdown()
		{
			mBackendRHI.Shutdown();
		}

		// TODO: REMOVE
		BackendRHI& GetBackendRHI() { return mBackendRHI; }

	private:
		BackendRHI mBackendRHI;
	};
}
