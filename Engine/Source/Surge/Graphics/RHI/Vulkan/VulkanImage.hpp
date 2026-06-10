// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RHI/RHIDescs.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanResourceEntries.hpp"
#include <volk.h>

namespace Surge
{
    class VulkanRHI;
    class VulkanImage
    {
    public:
        static ImageEntry Create(const VulkanRHI& rhi, const ImageDesc& desc);
        static void Destroy(const VulkanRHI& rhi, ImageEntry& entry);
        static void UploadData(VulkanRHI& rhi, ImageHandle h, const void* data, Uint size);
        static void UploadCompressedData(VulkanRHI& rhi, ImageHandle h, const MipUploadData* mips, Uint mipCount);

        // Record a pipeline barrier that transitions the image layout, Updates entry.Layout to newLayout after recording
        static void TransitionLayout(VkCommandBuffer cmd, ImageEntry& entry, VkImageLayout newLayout);
    private:
        static void GenerateMipmaps(VulkanRHI& rhi, VkCommandBuffer cmd, ImageEntry& entry);
    };

}