// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/RHI/Vulkan/VulkanUtils.hpp"

namespace Surge::VulkanUtils
{

	VkFormat ToVkVertexFormat(VertexFormat f)
	{
		switch (f)
		{
		case VertexFormat::FLOAT:  return VK_FORMAT_R32_SFLOAT;
		case VertexFormat::FLOAT2: return VK_FORMAT_R32G32_SFLOAT;
		case VertexFormat::FLOAT3: return VK_FORMAT_R32G32B32_SFLOAT;
		case VertexFormat::FLOAT4: return VK_FORMAT_R32G32B32A32_SFLOAT;
		case VertexFormat::INT:    return VK_FORMAT_R32_SINT;
		case VertexFormat::INT2:   return VK_FORMAT_R32G32_SINT;
		case VertexFormat::INT3:   return VK_FORMAT_R32G32B32_SINT;
		case VertexFormat::INT4:   return VK_FORMAT_R32G32B32A32_SINT;
		}
		return VK_FORMAT_UNDEFINED;
	}

	VkCullModeFlags ToVkCullMode(CullMode c)
	{
		switch (c)
		{
		case CullMode::NONE:  return VK_CULL_MODE_NONE;
		case CullMode::FRONT: return VK_CULL_MODE_FRONT_BIT;
		case CullMode::BACK:  return VK_CULL_MODE_BACK_BIT;
		}
		return VK_CULL_MODE_NONE;
	}

	VkFrontFace ToVkFrontFace(FrontFace f)
	{
		switch (f)
		{
		case FrontFace::CLOCKWISE:         return VK_FRONT_FACE_CLOCKWISE;
		case FrontFace::COUNTER_CLOCKWISE: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
		}
		return VK_FRONT_FACE_CLOCKWISE;
	}

	VkPrimitiveTopology ToVkTopology(Topology t)
	{
		switch (t)
		{
		case Topology::TRIANGLE_LIST:  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		case Topology::TRIANGLE_STRIP: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		case Topology::LINE_LIST:      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
		case Topology::POINT_LIST:     return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		}
		return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	}

	VkCompareOp ToVkCompareOp(CompareOp op)
	{
		switch (op)
		{
		case CompareOp::NEVER:            return VK_COMPARE_OP_NEVER;
		case CompareOp::LESS:             return VK_COMPARE_OP_LESS;
		case CompareOp::EQUAL:            return VK_COMPARE_OP_EQUAL;
		case CompareOp::LESS_OR_EQUAL:    return VK_COMPARE_OP_LESS_OR_EQUAL;
		case CompareOp::GREATER:          return VK_COMPARE_OP_GREATER;
		case CompareOp::NOT_EQUAL:        return VK_COMPARE_OP_NOT_EQUAL;
		case CompareOp::GREATER_OR_EQUAL: return VK_COMPARE_OP_GREATER_OR_EQUAL;
		case CompareOp::ALWAYS:           return VK_COMPARE_OP_ALWAYS;
		}
		return VK_COMPARE_OP_LESS;
	}

	VkBlendFactor ToVkBlendFactor(BlendFactor f)
	{
		switch (f)
		{
		case BlendFactor::ZERO:                return VK_BLEND_FACTOR_ZERO;
		case BlendFactor::ONE:                 return VK_BLEND_FACTOR_ONE;
		case BlendFactor::SRC_ALPHA:           return VK_BLEND_FACTOR_SRC_ALPHA;
		case BlendFactor::ONE_MINUS_SRC_ALPHA: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		case BlendFactor::DST_ALPHA:           return VK_BLEND_FACTOR_DST_ALPHA;
		case BlendFactor::ONE_MINUS_DST_ALPHA: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
		}
		return VK_BLEND_FACTOR_ZERO;
	}

	VkBlendOp ToVkBlendOp(BlendOp op)
	{
		switch (op)
		{
		case BlendOp::ADD:              return VK_BLEND_OP_ADD;
		case BlendOp::SUBTRACT:         return VK_BLEND_OP_SUBTRACT;
		case BlendOp::REVERSE_SUBTRACT: return VK_BLEND_OP_REVERSE_SUBTRACT;
		case BlendOp::MIN:              return VK_BLEND_OP_MIN;
		case BlendOp::MAX:              return VK_BLEND_OP_MAX;
		}
		return VK_BLEND_OP_ADD;
	}

	VkFormat TextureFormatToVkFormat(TextureFormat format)
	{
		switch (format)
		{
		case TextureFormat::RGBA8_SRGB:  return VK_FORMAT_R8G8B8A8_SRGB;
		case TextureFormat::RGBA8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
		case TextureFormat::BGRA8_SRGB:  return VK_FORMAT_B8G8R8A8_SRGB;
		case TextureFormat::D24_UNORM_S8_UINT:  return VK_FORMAT_D24_UNORM_S8_UINT;
		case TextureFormat::D32_SFLOAT:  return VK_FORMAT_D32_SFLOAT;
		case TextureFormat::R16G16B16A16_SFLOAT: return VK_FORMAT_R16G16B16A16_SFLOAT;
		default:
			SG_ASSERT_INTERNAL("Unsupported TextureFormat !");
		}
		return VK_FORMAT_UNDEFINED;
	}


	bool IsDepthFormat(TextureFormat format)
	{
		switch (format)
		{
		case TextureFormat::D32_SFLOAT:
		case TextureFormat::D24_UNORM_S8_UINT:
			return true;
		default:
			return false;
		}
	}

	VkImageUsageFlags ToVkImageUsage(TextureUsage usage, bool transient)
	{
		VkImageUsageFlags flags = 0;

		if ((Uint)usage & (Uint)TextureUsage::SAMPLED)
			flags |= VK_IMAGE_USAGE_SAMPLED_BIT;

		if ((Uint)usage & (Uint)TextureUsage::COLOR_ATTACHMENT)
			flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		if ((Uint)usage & (Uint)TextureUsage::DEPTH_ATTACHMENT)
			flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

		if ((Uint)usage & (Uint)TextureUsage::STORAGE)
			flags |= VK_IMAGE_USAGE_STORAGE_BIT;

		if ((Uint)usage & (Uint)TextureUsage::TRANSFER_SRC)
			flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

		if ((Uint)usage & (Uint)TextureUsage::TRANSFER_DST)
			flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		// Transient attachments on TBDR data lives on tile, never hits DRAM
		// Must NOT combine with SAMPLED or TRANSFER_SRC/DST
		if (transient)
			flags |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;

		return flags;
	}

	VkAttachmentLoadOp ToVkLoadOp(LoadOp op)
	{
		switch (op)
		{
		case LoadOp::CLEAR:     return VK_ATTACHMENT_LOAD_OP_CLEAR;
		case LoadOp::LOAD:      return VK_ATTACHMENT_LOAD_OP_LOAD;
		case LoadOp::DONT_CARE: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		}
		return VK_ATTACHMENT_LOAD_OP_CLEAR;
	}

	VkAttachmentStoreOp ToVkStoreOp(StoreOp op)
	{
		switch (op)
		{
		case StoreOp::STORE:     return VK_ATTACHMENT_STORE_OP_STORE;
		case StoreOp::DONT_CARE: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
		}
		return VK_ATTACHMENT_STORE_OP_STORE;
	}

	const char* BufferUsageToString(BufferUsage usage)
	{
		static thread_local char buffer[64];
		char* ptr = buffer;
		auto bits = static_cast<Uint>(usage);
		bool first = true;

		auto append = [&](const char* str)
			{
				if (!first)
				{
					*ptr++ = ' ';
					*ptr++ = '|';
					*ptr++ = ' ';
				}
				while (*str)
					*ptr++ = *str++;
				first = false;
			};

		if (bits & (Uint)BufferUsage::VERTEX)   append("VERTEX");
		if (bits & (Uint)BufferUsage::INDEX)    append("INDEX");
		if (bits & (Uint)BufferUsage::UNIFORM)  append("UNIFORM");
		if (bits & (Uint)BufferUsage::STORAGE)  append("STORAGE");
		if (bits & (Uint)BufferUsage::STAGING)  append("STAGING");

		if (first)
			append("NONE");

		*ptr = '\0'; // Null terminate the string
		return buffer;
	}

	const char* LoadOpToString(LoadOp op)
	{
		switch (op)
		{
		case LoadOp::LOAD:  return "LOAD";
		case LoadOp::CLEAR: return "CLEAR";
		case LoadOp::DONT_CARE: return "DONT_CARE";
		}
		return "Unknown";
	}

	Surge::String TextureFormatToString(TextureFormat format)
	{
		switch (format)
		{
		case TextureFormat::RGBA8_SRGB:			return "RGBA8_SRGB";
		case TextureFormat::RGBA8_UNORM:			return "RGBA8_UNORM";
		case TextureFormat::BGRA8_SRGB:			return "BGRA8_SRGB";
		case TextureFormat::D32_SFLOAT:			return "DEPTH32_SFLOAT";
		case TextureFormat::D24_UNORM_S8_UINT:	return "D24_UNORM_S8_UINT";
		case TextureFormat::R16G16B16A16_SFLOAT: return "R16G16B16A16_SFLOAT";
		}
		SG_ASSERT_INTERNAL("Unknown TextureFormat enum value!");
		return "Unknown";
	}

	const char* TextureUsageToString(TextureUsage usage)
	{
		thread_local char buffer[128];
		char* ptr = buffer;
		auto bits = static_cast<Uint>(usage);
		bool first = true;

		auto append = [&](const char* str)
			{
				if (!first)
				{
					*ptr++ = ' ';
					*ptr++ = '|';
					*ptr++ = ' ';
				}
				while (*str)
					*ptr++ = *str++;
				first = false;
			};

		if (bits & (Uint)TextureUsage::SAMPLED)              append("SAMPLED");
		if (bits & (Uint)TextureUsage::COLOR_ATTACHMENT)     append("COLOR_ATTACHMENT");
		if (bits & (Uint)TextureUsage::DEPTH_ATTACHMENT)     append("DEPTH_ATTACHMENT");
		if (bits & (Uint)TextureUsage::TRANSIENT_ATTACHMENT) append("TRANSIENT_ATTACHMENT");
		if (bits & (Uint)TextureUsage::STORAGE)              append("STORAGE");
		if (bits & (Uint)TextureUsage::TRANSFER_SRC)         append("TRANSFER_SRC");
		if (bits & (Uint)TextureUsage::TRANSFER_DST)         append("TRANSFER_DST");

		if (first)
			append("NONE");

		*ptr = '\0'; // Null terminate
		return buffer;
	}

	const char* StoreOpToString(StoreOp op)
	{
		switch (op)
		{
		case StoreOp::STORE:  return "STORE";
		case StoreOp::DONT_CARE: return "DONT_CARE";
		}
		return "Unknown";
	}

}
