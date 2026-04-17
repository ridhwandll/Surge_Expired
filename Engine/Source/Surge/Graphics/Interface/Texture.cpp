// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/RHI/Vulkan/VulkanTexture.hpp"

namespace Surge
{
    Ref<Texture2D> Texture2D::Create(const String& filepath, TextureSpecification specification)
    {
        return Ref<VulkanTexture2D>::Create(filepath, specification);
    }

    Ref<Texture2D> Texture2D::Create(ImageFormat format, Uint width, Uint height, void* data, TextureSpecification specification)
    {
        return Ref<VulkanTexture2D>::Create(format, width, height, data, specification);
    }

    Uint Texture::CalculateMipChainLevels(Uint width, Uint height)
    {
        return static_cast<Uint>(std::floor(std::log2(std::max(width, height))) + 1);
    }

} // namespace Surge
