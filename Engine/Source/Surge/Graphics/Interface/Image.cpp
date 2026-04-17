// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/RHI/Vulkan/VulkanImage.hpp"

namespace Surge
{
    Ref<Image2D> Image2D::Create(const ImageSpecification& specification)
    {
        return Ref<VulkanImage2D>::Create(specification);
    }

} // namespace Surge
