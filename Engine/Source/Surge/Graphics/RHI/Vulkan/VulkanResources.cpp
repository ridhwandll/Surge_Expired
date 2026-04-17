// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/RHI/Vulkan/VulkanResources.hpp"

namespace Surge::RHI
{
    VulkanResourceRegistry& VulkanResourceRegistry::Get()
    {
        static VulkanResourceRegistry sInstance;
        return sInstance;
    }

    VulkanResourceRegistry& GetVulkanRegistry()
    {
        return VulkanResourceRegistry::Get();
    }

} // namespace Surge::RHI
