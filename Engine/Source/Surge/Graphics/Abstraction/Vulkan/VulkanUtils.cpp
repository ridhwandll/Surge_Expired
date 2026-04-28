// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Abstraction/Vulkan/VulkanUtils.hpp"
#include "Surge/Graphics/Abstraction/Vulkan/VulkanDiagnostics.hpp"
#ifdef SURGE_PLATFORM_ANDROID
#include <android/native_window.h>
#endif

namespace Surge
{
    ShaderType VulkanUtils::ShaderTypeFromString(const String& type)
    {
        if (type == "Vertex]")
            return ShaderType::Vertex;
        if (type == "Pixel]")
            return ShaderType::Pixel;
        if (type == "Compute]")
            return ShaderType::Compute;

        return ShaderType::None;
    }

    String VulkanUtils::ShaderTypeToString(const ShaderType& type)
    {
        switch (type)
        {
            case ShaderType::Vertex: return "Vertex";
            case ShaderType::Pixel: return "Pixel";
            case ShaderType::Compute: return "Compute";
            case ShaderType::None: SG_ASSERT_INTERNAL("ShaderType::None is invalid in this case!");
        }
        return "";
    }

    VkPrimitiveTopology VulkanUtils::GetVulkanPrimitiveTopology(PrimitiveTopology primitive)
    {
        switch (primitive)
        {
            case PrimitiveTopology::PointList: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
            case PrimitiveTopology::LineList: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
            case PrimitiveTopology::LineStrip: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
            case PrimitiveTopology::TriangleList: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            case PrimitiveTopology::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
            case PrimitiveTopology::None: SG_ASSERT_INTERNAL("PrimitiveType::None is invalid!");
        }

        SG_ASSERT_INTERNAL("No Surge::PrimitiveType maps to VkPrimitiveTopology!");
        return VkPrimitiveTopology();
    }

    VkFormat VulkanUtils::ShaderDataTypeToVulkanFormat(ShaderDataType type)
    {
        switch (type)
        {
            case ShaderDataType::Float: return VK_FORMAT_R32_SFLOAT;
            case ShaderDataType::Float2: return VK_FORMAT_R32G32_SFLOAT;
            case ShaderDataType::Float3: return VK_FORMAT_R32G32B32_SFLOAT;
            case ShaderDataType::Float4: return VK_FORMAT_R32G32B32A32_SFLOAT;
            default: SG_ASSERT_INTERNAL("Undefined!");
        }

        SG_ASSERT_INTERNAL("No Surge::ShaderDataType maps to VkFormat!");
        return VK_FORMAT_UNDEFINED;
    }

    Vector<VkPushConstantRange> VulkanUtils::GetPushConstantRangesVectorFromHashMap(const HashMap<String, VkPushConstantRange>& pushConstants)
    {
        Vector<VkPushConstantRange> pushConstantsVector;
        for (auto& pushConstant : pushConstants)
            pushConstantsVector.push_back(pushConstant.second);
        return pushConstantsVector;
    }

    VkDescriptorType VulkanUtils::ShaderBufferTypeToVulkan(ShaderBuffer::Usage type)
    {
        switch (type)
        {
            case ShaderBuffer::Usage::Storage: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            case ShaderBuffer::Usage::Uniform: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        }
        SG_ASSERT(false, "ShaderBuffer::Usage is invalid");
        return VkDescriptorType();
    }

    VkDescriptorType VulkanUtils::ShaderImageUsageToVulkan(ShaderResource::Usage type)
    {
        switch (type)
        {
            case ShaderResource::Usage::Sampled: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            case ShaderResource::Usage::Storage: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        }
        SG_ASSERT(false, "ShaderResource::Usage is invalid");
        return VkDescriptorType();
    }

    VkShaderStageFlags VulkanUtils::GetShaderStagesFlagsFromShaderTypes(ShaderType shaderStages)
    {
        VkShaderStageFlags stageFlags {};

        if (shaderStages & ShaderType::Vertex)
            stageFlags |= VK_SHADER_STAGE_VERTEX_BIT;
        if (shaderStages & ShaderType::Pixel)
            stageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;
        if (shaderStages & ShaderType::Compute)
            stageFlags |= VK_SHADER_STAGE_COMPUTE_BIT;
        if (shaderStages & ShaderType::None)
            SG_ASSERT_INTERNAL("ShaderType::None is invalid!");

        return stageFlags;
    }

    void VulkanUtils::CreateWindowSurface(VkInstance instance, Window* windowHandle, VkSurfaceKHR* surface)
    {
#ifdef SURGE_PLATFORM_WINDOWS
        PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR;

        // Getting the vkCreateWin32SurfaceKHR function pointer and assert if it doesnt exist
        vkCreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)vkGetInstanceProcAddr(instance, "vkCreateWin32SurfaceKHR");
        if (!vkCreateWin32SurfaceKHR)
            SG_ASSERT_INTERNAL("[Win32] Vulkan instance missing VK_KHR_win32_surface extension");

        VkWin32SurfaceCreateInfoKHR sci;
        memset(&sci, 0, sizeof(sci)); // Clear the info
        sci.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        sci.hinstance = GetModuleHandle(nullptr);
        sci.hwnd = static_cast<HWND>(windowHandle->GetNativeWindowHandle());

        VK_CALL(vkCreateWin32SurfaceKHR(instance, &sci, nullptr, surface));
#elif defined(SURGE_PLATFORM_ANDROID)
        PFN_vkCreateAndroidSurfaceKHR vkCreateAndroidSurfaceKHR = (PFN_vkCreateAndroidSurfaceKHR)vkGetInstanceProcAddr(instance, "vkCreateAndroidSurfaceKHR");
        if (!vkCreateAndroidSurfaceKHR)
            SG_ASSERT_INTERNAL("[Android] Vulkan instance missing VK_KHR_android_surface extension");

        VkAndroidSurfaceCreateInfoKHR sci {};
        sci.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
        sci.window = static_cast<ANativeWindow*>(windowHandle->GetNativeWindowHandle());

        VK_CALL(vkCreateAndroidSurfaceKHR(instance, &sci, nullptr, surface));
#else
        SG_ASSERT_INTERNAL("Surge is currently Windows/Android Only! :(");
#endif
    }

    VkFormat VulkanUtils::GetImageFormat(ImageFormat format)
    {
        switch (format)
        {
            case ImageFormat::RGBA32F: return VK_FORMAT_R32G32B32A32_SFLOAT;
            case ImageFormat::Depth32: return VK_FORMAT_D32_SFLOAT;
            case ImageFormat::RGBA8: return VK_FORMAT_R8G8B8A8_UNORM;
            case ImageFormat::RGBA16F: return VK_FORMAT_R16G16B16A16_SFLOAT;
            case ImageFormat::Depth24Stencil8: return VK_FORMAT_D24_UNORM_S8_UINT;
            case ImageFormat::RED32F: return VK_FORMAT_R32_SFLOAT;
            case ImageFormat::None: SG_ASSERT_INTERNAL("ImageFormat::None is invalid!");
        }
        SG_ASSERT_INTERNAL("Invalid ImageFormat!");
        return VK_FORMAT_UNDEFINED;
    }

    VkFilter VulkanUtils::GetImageFiltering(TextureFilter filtering)
    {
        switch (filtering)
        {
            case TextureFilter::Linear: return VK_FILTER_LINEAR;
            case TextureFilter::Nearest: return VK_FILTER_NEAREST;
        }
        SG_ASSERT_INTERNAL("Invalid TextureFilter!");
        return VK_FILTER_MAX_ENUM;
    }

    VkSamplerAddressMode VulkanUtils::GetImageAddressMode(TextureAddressMode wrap)
    {
        switch (wrap)
        {
            case TextureAddressMode::Repeat: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
            case TextureAddressMode::ClampToBorder: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
            case TextureAddressMode::ClampToEdge: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            case TextureAddressMode::MirroredRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        }
        SG_ASSERT_INTERNAL("Invalid TextureAddressMode!");
        return VK_SAMPLER_ADDRESS_MODE_MAX_ENUM;
    }

    VkImageUsageFlags VulkanUtils::GetImageUsageFlags(ImageUsage usage, ImageFormat format)
    {
        VkImageUsageFlags vkImageUsage = VK_IMAGE_USAGE_SAMPLED_BIT; // TODO: Maybe not force this?
        switch (usage)
        {
            case ImageUsage::Attachment:
            {
                if (VulkanUtils::IsDepthFormat(format))
                    vkImageUsage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
                else
                    vkImageUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
                break;
            }
            case ImageUsage::Texture: vkImageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT; break;
            case ImageUsage::Storage: vkImageUsage |= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT; break;
            case ImageUsage::None: SG_ASSERT_INTERNAL("ImageUsage::None is invalid!"); break;
        }

        return vkImageUsage;
    }

    void VulkanUtils::InsertImageMemoryBarrier(VkCommandBuffer cmdbuffer, VkImage& image,
                                               VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
                                               VkImageLayout oldImageLayout, VkImageLayout newImageLayout,
                                               VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,
                                               VkImageSubresourceRange subresourceRange)
    {
        VkImageMemoryBarrier imageMemoryBarrier {};
        imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        imageMemoryBarrier.srcAccessMask = srcAccessMask;
        imageMemoryBarrier.dstAccessMask = dstAccessMask;
        imageMemoryBarrier.oldLayout = oldImageLayout;
        imageMemoryBarrier.newLayout = newImageLayout;
        imageMemoryBarrier.image = image;
        imageMemoryBarrier.subresourceRange = subresourceRange;
        vkCmdPipelineBarrier(cmdbuffer, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
    }
    VkShaderStageFlagBits VulkanUtils::GetVulkanShaderStage(ShaderType type)
    {
        switch (type)
        {
            case ShaderType::Vertex: return VK_SHADER_STAGE_VERTEX_BIT;
            case ShaderType::Pixel: return VK_SHADER_STAGE_FRAGMENT_BIT;
            case ShaderType::Compute: return VK_SHADER_STAGE_COMPUTE_BIT;
            case ShaderType::None: SG_ASSERT_INTERNAL("ShaderType::None is invalid in this case!");
        }
        SG_ASSERT_INTERNAL("Invalid shader type specified!");
        return VkShaderStageFlagBits();
    }

    VkPolygonMode VulkanUtils::GetVulkanPolygonMode(PolygonMode mode)
    {
        switch (mode)
        {
            case PolygonMode::Fill: return VK_POLYGON_MODE_FILL;
            case PolygonMode::Line: return VK_POLYGON_MODE_LINE;
            case PolygonMode::Point: return VK_POLYGON_MODE_POINT;
            case PolygonMode::None: SG_ASSERT_INTERNAL("PolygonMode::None is invalid in this case!");
        }
        SG_ASSERT_INTERNAL("Invalid polygon mode specified!");
        return VK_POLYGON_MODE_MAX_ENUM;
    }

    VkCullModeFlags VulkanUtils::GetVulkanCullModeFlags(CullMode mode)
    {
        switch (mode)
        {
            case CullMode::Back: return VK_CULL_MODE_BACK_BIT;
            case CullMode::Front: return VK_CULL_MODE_FRONT_BIT;
            case CullMode::FrontAndBack: return VK_CULL_MODE_FRONT_AND_BACK;
            case CullMode::None: return VK_CULL_MODE_NONE;
        }
        SG_ASSERT_INTERNAL("Invalid cull mode specified!");
        return VK_CULL_MODE_FLAG_BITS_MAX_ENUM;
    }

    VkCompareOp VulkanUtils::GetVulkanCompareOp(CompareOperation op)
    {
        switch (op)
        {
            case CompareOperation::Never: return VK_COMPARE_OP_NEVER;
            case CompareOperation::Less: return VK_COMPARE_OP_LESS;
            case CompareOperation::Equal: return VK_COMPARE_OP_EQUAL;
            case CompareOperation::LessOrEqual: return VK_COMPARE_OP_LESS_OR_EQUAL;
            case CompareOperation::Greater: return VK_COMPARE_OP_GREATER;
            case CompareOperation::NotEqual: return VK_COMPARE_OP_NOT_EQUAL;
            case CompareOperation::GreaterOrEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
            case CompareOperation::Always: return VK_COMPARE_OP_ALWAYS;
        }
        SG_ASSERT_INTERNAL("Invalid DepthCompareOperation specified!");
        return VK_COMPARE_OP_MAX_ENUM;
    }

    bool VulkanUtils::IsDepthFormat(ImageFormat imageFormat)
    {
        switch (imageFormat)
        {
            case ImageFormat::RED32F:
            case ImageFormat::RGBA8:
            case ImageFormat::RGBA16F:
            case ImageFormat::RGBA32F:
                return false;
            case ImageFormat::Depth32:
            case ImageFormat::Depth24Stencil8:
                return true;
            case ImageFormat::None: SG_ASSERT_INTERNAL("ImageFormat::None is invalid in this case!"); break;
        }
        SG_ASSERT_INTERNAL("ImageFormat is invalid!");
        return false;
    }

    Uint VulkanUtils::GetMemorySize(ImageFormat format, Uint width, Uint height)
    {
        switch (format)
        {
            case ImageFormat::RGBA8: return width * height * (4);                   // 32 bit(4 byte) per pixel
            case ImageFormat::RGBA16F: return width * height * (4 * 2);             // 64 bit(8 byte) per pixel
            case ImageFormat::RGBA32F: return width * height * (4 * sizeof(float)); // 128 bit(16 byte) per pixel
        }
        SG_ASSERT_INTERNAL("Invalid ImageFormat!");
        return 0;
    }

    Vector<VkDescriptorSetLayout> VulkanUtils::GetDescriptorSetLayoutVectorFromMap(const std::map<Uint, VkDescriptorSetLayout>& layouts)
    {
        Vector<VkDescriptorSetLayout> result;
        for (auto& layout : layouts)
            result.push_back(layout.second);
        return result;
    }

    VkCompareOp VulkanUtils::CompareOpToVkCompareOp(CompareOp op)
    {
        switch (op)
        {
            case Surge::CompareOp::Never: return VK_COMPARE_OP_NEVER;
            case Surge::CompareOp::Less: return VK_COMPARE_OP_LESS;
            case Surge::CompareOp::Equal: return VK_COMPARE_OP_EQUAL;
            case Surge::CompareOp::LessOrEqual: return VK_COMPARE_OP_LESS_OR_EQUAL;
            case Surge::CompareOp::Greater: return VK_COMPARE_OP_GREATER;
            case Surge::CompareOp::NotEqual: return VK_COMPARE_OP_NOT_EQUAL;
            case Surge::CompareOp::GreaterOrEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
            case Surge::CompareOp::Always: return VK_COMPARE_OP_ALWAYS;
        }
        SG_ASSERT_INTERNAL("Invalid CompareOp!");
        return VK_COMPARE_OP_MAX_ENUM;
    }

} // namespace Surge
