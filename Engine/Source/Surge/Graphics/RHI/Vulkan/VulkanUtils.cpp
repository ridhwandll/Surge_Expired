// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Shader/Shader.hpp"
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

		if (usage & TextureUsage::SAMPLED)
			flags |= VK_IMAGE_USAGE_SAMPLED_BIT;

		if (usage & TextureUsage::COLOR_ATTACHMENT)
			flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		if (usage & TextureUsage::DEPTH_ATTACHMENT)
			flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

		if (usage & TextureUsage::STORAGE)
			flags |= VK_IMAGE_USAGE_STORAGE_BIT;

		if (usage & TextureUsage::TRANSFER_SRC)
			flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

		if (usage & TextureUsage::TRANSFER_DST)
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

	String ShaderDataTypeToString(const ShaderDataType& type)
	{
		switch (type)
		{
		case ShaderDataType::INT: return "Int";
		case ShaderDataType::UINT: return "UInt";
		case ShaderDataType::FLOAT: return "Float";
		case ShaderDataType::FLOAT2: return "Float2";
		case ShaderDataType::FLOAT3: return "Float3";
		case ShaderDataType::FLOAT4: return "Float4";
		case ShaderDataType::MAT2: return "Mat2";
		case ShaderDataType::MAT4: return "Mat4";
		case ShaderDataType::MAT3: return "Mat3";
		case ShaderDataType::BOOL: return "Bool";
		case ShaderDataType::STRUCT: return "Struct";
		case ShaderDataType::NONE: SG_ASSERT_INTERNAL("ShaderDataType::NONE is invalid in this case!");
		default:
			SG_ASSERT_INTERNAL("Tf?");
		}
		SG_ASSERT_INTERNAL("Unknown ShaderDataType!");
		return "NONE";
	}

	Uint ShaderDataTypeSize(ShaderDataType type)
	{
		switch (type)
		{
		case ShaderDataType::FLOAT: return 4;
		case ShaderDataType::FLOAT2: return 4 * 2;
		case ShaderDataType::FLOAT3: return 4 * 3;
		case ShaderDataType::FLOAT4: return 4 * 4;
		case ShaderDataType::MAT3: return 4 * 3 * 3;
		case ShaderDataType::MAT4: return 4 * 4 * 4;
		case ShaderDataType::INT: return 4;
		case ShaderDataType::UINT: return 4;
		case ShaderDataType::BOOL: return 4;
		case ShaderDataType::STRUCT: return -1;
		default: SG_ASSERT_INTERNAL("Invalid case!");
		}

		SG_ASSERT_INTERNAL("Unknown ShaderDataType!");
		return 0;
	}

	String ShaderTypeToString(const ShaderType& type)
	{
		switch (type)
		{
		case ShaderType::VERTEX: return "Vertex";
		case ShaderType::FRAGMENT: return "Fragment";
		case ShaderType::COMPUTE: return "Compute";
		case ShaderType::NONE: SG_ASSERT_INTERNAL("ShaderType::NONE is invalid in this case!");
		}
		SG_ASSERT_INTERNAL("Unknown ShaderType!");
		return "NONE";
	}

	ShaderType ShaderTypeFromString(const String& type)
	{
		
		if (type == "None") return ShaderType::NONE;
		else if (type == "Vertex") return ShaderType::VERTEX;
		else if (type == "Pixel") return ShaderType::FRAGMENT; // Backwards compatibility
		else if (type == "Fragment") return ShaderType::FRAGMENT;		
		else if (type == "Compute") return ShaderType::COMPUTE;
		else
		{
			 SG_ASSERT_INTERNAL("Unknown shader type string!");
			 return ShaderType::NONE;
		}		
	}

	VkFormat ShaderDataTypeToVulkanFormat(ShaderDataType type)
	{
		switch (type)		
		{
		case ShaderDataType::UINT: return VK_FORMAT_R32_UINT;
		case ShaderDataType::FLOAT: return VK_FORMAT_R32_SFLOAT;
		case ShaderDataType::FLOAT2: return VK_FORMAT_R32G32_SFLOAT;
		case ShaderDataType::FLOAT3: return VK_FORMAT_R32G32B32_SFLOAT;
		case ShaderDataType::FLOAT4: return VK_FORMAT_R32G32B32A32_SFLOAT;
		default: SG_ASSERT_INTERNAL("Undefined!");
		}

		SG_ASSERT_INTERNAL("No Surge::ShaderDataType maps to VkFormat!");
		return VK_FORMAT_UNDEFINED;
	}

	VkShaderStageFlags ShaderTypeToVulkanShaderStage(ShaderType type)
	{
		VkShaderStageFlags flags = 0;

		if (type & ShaderType::VERTEX)   flags |= VK_SHADER_STAGE_VERTEX_BIT;
		if (type & ShaderType::FRAGMENT) flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
		if (type & ShaderType::COMPUTE)  flags |= VK_SHADER_STAGE_COMPUTE_BIT;

		return flags;
	}

	VkFilter ToVkFilter(FilterMode m)
	{
		return m == FilterMode::LINEAR ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
	}

	VkSamplerAddressMode ToVkWrap(WrapMode m)
	{
		switch (m)
		{
		case WrapMode::REPEAT:          return VK_SAMPLER_ADDRESS_MODE_REPEAT;
		case WrapMode::CLAMP:           return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		case WrapMode::MIRRORED_REPEAT: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
		}
		return VK_SAMPLER_ADDRESS_MODE_REPEAT;
	}

	VkDescriptorType ToVkDescriptorType(DescriptorType type)
	{
		switch (type)
		{
		case DescriptorType::TEXTURE:         return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		case DescriptorType::STORAGE_TEXTURE: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		case DescriptorType::UNIFORM_BUFFER:  return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		case DescriptorType::STORAGE_BUFFER:  return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		case DescriptorType::SAMPLER:         return VK_DESCRIPTOR_TYPE_SAMPLER;
		}

		SG_ASSERT_INTERNAL("Unknown DescriptorType!");
		return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	}

	VkShaderStageFlags ToVkShaderStage(ShaderType shaderType)
	{
		VkShaderStageFlags flags = 0;
		if ((Uint)shaderType & (Uint)ShaderType::VERTEX)   flags |= VK_SHADER_STAGE_VERTEX_BIT;
		if ((Uint)shaderType & (Uint)ShaderType::FRAGMENT)  flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
		if ((Uint)shaderType & (Uint)ShaderType::COMPUTE)   flags |= VK_SHADER_STAGE_COMPUTE_BIT;
		return flags;

	}

	VkPolygonMode ToVkPolygonMode(PolygonMode p)
	{
		switch (p)
		{
		case PolygonMode::FILL:  return VK_POLYGON_MODE_FILL;
		case PolygonMode::LINE:  return VK_POLYGON_MODE_LINE;
		case PolygonMode::POINT: return VK_POLYGON_MODE_POINT;
		default:
			SG_ASSERT_INTERNAL("Unknown PolygonMode!");
		}
	}

	VkImageLayout TextureUsageToVkLayout(TextureUsage usage)
	{
		switch (usage)
		{
		case TextureUsage::SAMPLED:          return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		case TextureUsage::COLOR_ATTACHMENT: return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		case TextureUsage::DEPTH_ATTACHMENT: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		case TextureUsage::STORAGE:			 return VK_IMAGE_LAYOUT_GENERAL;
		case TextureUsage::TRANSFER_SRC:     return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		case TextureUsage::TRANSFER_DST:     return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		case TextureUsage::TRANSIENT_ATTACHMENT:
			SG_ASSERT_INTERNAL("I dont know what image layout to add here");
		default:
			// Always safe as a fallback, but can be slower
			Log<Severity::Warn>("TextureUsageToVkLayout: unhandled TextureUsage flag, defaulting to VK_IMAGE_LAYOUT_GENERAL. This may cause performance issues.");
			return VK_IMAGE_LAYOUT_GENERAL;
		}
	}

}
