// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RHI/RHIDescs.hpp"
#include <volk.h>


namespace Surge::VulkanUtils
{
    VkImageLayout ImageUsageToVkLayout(ImageUsage usage);
    VkFormat ToVkVertexFormat(VertexFormat f);
    VkCullModeFlags ToVkCullMode(CullMode c);
    VkFrontFace ToVkFrontFace(FrontFace f);
    VkPrimitiveTopology ToVkTopology(Topology t);
    VkPolygonMode ToVkPolygonMode(PolygonMode p);
    VkCompareOp ToVkCompareOp(CompareOp op);
    VkBlendFactor ToVkBlendFactor(BlendFactor f);
    VkBlendOp ToVkBlendOp(BlendOp op);
    VkAttachmentLoadOp ToVkLoadOp(LoadOp op);
    VkAttachmentStoreOp ToVkStoreOp(StoreOp op);

    VkFormat TextureFormatToVkFormat(ImageFormat format);
    bool IsDepthFormat(ImageFormat format);
    VkImageUsageFlags ToVkImageUsage(ImageUsage usage, bool transient);

    String TextureFormatToString(ImageFormat format);
    const char* TextureUsageToString(ImageUsage usage);
    const char* BufferUsageToString(BufferUsage usage);
    const char* LoadOpToString(LoadOp op);
    const char* StoreOpToString(StoreOp op);

    String ShaderDataTypeToString(const ShaderDataType& type);
    Uint ShaderDataTypeSize(ShaderDataType type);
    String ShaderTypeToString(const ShaderType& type);
    ShaderType ShaderTypeFromString(const String& type);
    VkFormat ShaderDataTypeToVulkanFormat(ShaderDataType type);
    VkShaderStageFlags ShaderTypeToVulkanShaderStage(ShaderType type);

    VkFilter ToVkFilter(FilterMode m);
    VkSamplerAddressMode ToVkWrap(WrapMode m);

    VkDescriptorType ToVkDescriptorType(DescriptorType type);
}