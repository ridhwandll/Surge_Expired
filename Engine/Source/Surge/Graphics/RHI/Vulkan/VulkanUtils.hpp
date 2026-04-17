// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/Interface/Image.hpp"
#include "Surge/Graphics/Shader/ReflectionData.hpp"
#include "Surge/Graphics/Shader/Shader.hpp"
#include <shaderc/shaderc.h>

namespace Surge::VulkanUtils
{
    ShaderType ShaderTypeFromString(const String& type);
    String ShaderTypeToString(const ShaderType& type);
    shaderc_shader_kind ShadercShaderKindFromSurgeShaderType(const ShaderType& type);

    VkCompareOp CompareOpToVkCompareOp(CompareOp op);
    VkFormat ShaderDataTypeToVulkanFormat(ShaderDataType type);
    Vector<VkPushConstantRange> GetPushConstantRangesVectorFromHashMap(const HashMap<String, VkPushConstantRange>& pushConstants);
    Vector<VkDescriptorSetLayout> GetDescriptorSetLayoutVectorFromMap(const std::map<Uint, VkDescriptorSetLayout>& layouts);
    VkDescriptorType ShaderBufferTypeToVulkan(ShaderBuffer::Usage type);
    VkDescriptorType ShaderImageUsageToVulkan(ShaderResource::Usage type);
    VkShaderStageFlags GetShaderStagesFlagsFromShaderTypes(ShaderType shaderStages);
    VkShaderStageFlagBits GetVulkanShaderStage(ShaderType type);
    bool IsDepthFormat(ImageFormat imageFormat);
    Uint GetMemorySize(ImageFormat format, Uint width, Uint height);

    // Image Related
    VkFormat GetImageFormat(ImageFormat format);
    VkFilter GetImageFiltering(TextureFilter filtering);
    VkSamplerAddressMode GetImageAddressMode(TextureAddressMode wrap);
    VkImageUsageFlags GetImageUsageFlags(ImageUsage usage, ImageFormat format);

    void InsertImageMemoryBarrier(VkCommandBuffer cmdbuffer, VkImage& image,
                                  VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
                                  VkImageLayout oldImageLayout, VkImageLayout newImageLayout,
                                  VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,
                                  VkImageSubresourceRange subresourceRange);

} // namespace Surge::VulkanUtils
