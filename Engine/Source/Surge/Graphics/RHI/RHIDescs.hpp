// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Graphics/RHI/RHIHandle.hpp"
#include "Surge/Graphics/Shader/Shader.hpp"
#include <array>

namespace Surge
{
	struct RHIStats
	{
		String GPUName;
		String RHIVersion;
		String VendorName;
		Uint DrawCalls = 0;

		uint64_t AllocationCount;
		uint64_t UsedGPUMemory;
		uint64_t TotalAllowedGPUMemory;

		void Reset()
		{
			DrawCalls = 0;
		}
	};

	enum class BufferUsage : Uint
	{
		VERTEX = BIT(0),
		INDEX = BIT(1),
		UNIFORM = BIT(2),
		STORAGE = BIT(3),
		STAGING = BIT(4),
	};
	MAKE_BIT_ENUM(BufferUsage, Uint);

	enum class TextureFormat
	{
		RGBA8_SRGB,
		RGBA8_UNORM,
		BGRA8_SRGB,
		D32_SFLOAT,
		D24_UNORM_S8_UINT,
		R16G16B16A16_SFLOAT,
	};

	enum class TextureUsage : Uint
	{
		SAMPLED = BIT(0),
		COLOR_ATTACHMENT = BIT(1),
		DEPTH_ATTACHMENT = BIT(2),
		TRANSIENT_ATTACHMENT = BIT(3),
		STORAGE = BIT(4),
		TRANSFER_SRC = BIT(5),
		TRANSFER_DST = BIT(6)
	};
	MAKE_BIT_ENUM(TextureUsage, Uint);

	enum class LoadOp { CLEAR, LOAD, DONT_CARE };
	enum class StoreOp { STORE, DONT_CARE };

	//Descriptors 

	struct BufferDesc
	{
		uint64_t Size = 0;
		const void* InitialData = nullptr;
		BufferUsage Usage = BufferUsage::VERTEX;
		bool HostVisible = false;
		const char* DebugName = nullptr;
	};

	struct TextureDesc
	{
		Uint Width = 1;
		Uint Height = 1;
		Uint Mips = 1;
		Uint Layers = 1;
		TextureFormat Format = TextureFormat::RGBA8_SRGB;
		TextureUsage Usage = TextureUsage::SAMPLED;
		bool Transient = false;
		SamplerHandle Sampler = {};

		// Optional, if set, pixel data is uploaded to GPU immediately after creation
		// Usage must include TRANSFER_DST if InitialData is provided
		const void* InitialData = nullptr;
		Uint DataSize = 0; // total bytes of InitialData

		String DebugName = "TextureHandle";
	};

	struct FramebufferAttachment
	{
		TextureHandle Handle;
		LoadOp Load = LoadOp::CLEAR;
		StoreOp Store = StoreOp::STORE;
	};

	struct FramebufferDesc
	{
		std::array<FramebufferAttachment, 8> ColorAttachments = {};
		Uint ColorAttachmentCount = 0;

		FramebufferAttachment DepthAttachment = {};
		bool HasDepth = false;
		Uint Width = 0;
		Uint Height = 0;

		const char* DebugName = nullptr;
	};

	enum class VertexFormat
	{
		FLOAT,   // R32_SFLOAT
		FLOAT2,  // R32G32_SFLOAT
		FLOAT3,  // R32G32B32_SFLOAT
		FLOAT4,  // R32G32B32A32_SFLOAT
		INT,     // R32_SINT
		INT2,    // R32G32_SINT
		INT3,    // R32G32B32_SINT
		INT4,    // R32G32B32A32_SINT
	};

	struct VertexAttribute
	{
		Uint Location = 0;
		Uint Binding = 0;
		VertexFormat Format = VertexFormat::FLOAT3;
		Uint Offset = 0;
	};

	struct VertexBinding
	{
		Uint Binding = 0;
		Uint Stride = 0;
	};

	enum class CullMode { NONE, FRONT, BACK };
	enum class FrontFace { CLOCKWISE, COUNTER_CLOCKWISE };
	enum class Topology { TRIANGLE_LIST, TRIANGLE_STRIP, LINE_LIST, POINT_LIST };
	enum class PolygonMode { FILL, LINE, POINT };

	struct RasterDesc
	{
		CullMode Cull = CullMode::BACK;
		FrontFace Front = FrontFace::COUNTER_CLOCKWISE;
		Topology Topo = Topology::TRIANGLE_LIST;
		PolygonMode Polygon = PolygonMode::FILL;
		float LineWidth = 1.0f;
		bool DepthClamp = false;
	};

	enum class CompareOp
	{
		NEVER, LESS, EQUAL, LESS_OR_EQUAL,
		GREATER, NOT_EQUAL, GREATER_OR_EQUAL, ALWAYS
	};

	struct DepthDesc
	{
		bool TestEnable = false;
		bool WriteEnable = false;
		CompareOp Op = CompareOp::LESS;
	};

	enum class BlendFactor
	{
		ZERO, ONE,
		SRC_ALPHA, ONE_MINUS_SRC_ALPHA,
		DST_ALPHA, ONE_MINUS_DST_ALPHA,
	};

	enum class BlendOp { ADD, SUBTRACT, REVERSE_SUBTRACT, MIN, MAX };

	struct BlendDesc
	{
		bool Enable = false;
		BlendFactor SrcColor = BlendFactor::SRC_ALPHA;
		BlendFactor DstColor = BlendFactor::ONE_MINUS_SRC_ALPHA;
		BlendOp ColorOp = BlendOp::ADD;
		BlendFactor SrcAlpha = BlendFactor::ONE;
		BlendFactor DstAlpha = BlendFactor::ZERO;
		BlendOp AlphaOp = BlendOp::ADD;
	};

	struct PipelineDesc
	{
		// Shaders
		Shader Shader_; // Attributes and bindings are reflected from shader via SPIRV-Cross

		// State
		RasterDesc Raster = {};
		DepthDesc Depth = {};
		BlendDesc Blend = {};

		FramebufferHandle TargetFramebuffer = FramebufferHandle::Invalid(); // Offscreen
		bool TargetSwapchain = false; // Swapchain pass

		const char* DebugName = nullptr;
	};

	enum class FilterMode { NEAREST, LINEAR };
	enum class WrapMode { REPEAT, CLAMP, MIRRORED_REPEAT };
	enum class MipmapMode { NEAREST, LINEAR };

	struct SamplerDesc
	{
		FilterMode Min = FilterMode::LINEAR;
		FilterMode Mag = FilterMode::LINEAR;
		MipmapMode Mip = MipmapMode::LINEAR;
		WrapMode WrapU = WrapMode::REPEAT;
		WrapMode WrapV = WrapMode::REPEAT;
		float MipBias = 0.0f;
		float MaxAniso = 1.0f;
		bool Anisotropy = false;
		const char* DebugName = nullptr;
	};

	enum class DescriptorType : Uint
	{
		TEXTURE,         // combined image + sampler
		STORAGE_TEXTURE, // read/write image
		UNIFORM_BUFFER,  // small read-only buffer
		STORAGE_BUFFER,  // large read/write buffer
		SAMPLER,         // separate sampler
	};

	struct DescriptorBinding
	{
		Uint Slot = 0;
		DescriptorType Type = DescriptorType::TEXTURE;
		Uint Count = 1; // >1 for arrays
		ShaderType Stage = ShaderType::FRAGMENT;
		bool Partial = false; // allow partially bound arrays
	};

	struct DescriptorLayoutDesc
	{
		DescriptorBinding Bindings[16] = {};
		Uint BindingCount = 0;
		const char* DebugName = nullptr;
	};

	struct DescriptorWrite
	{
		Uint Slot = 0;
		Uint ArrayIndex = 0; // for array bindings
		DescriptorType Type = DescriptorType::TEXTURE;

		TextureHandle Texture = TextureHandle::Invalid();
		SamplerHandle Sampler = SamplerHandle::Invalid();
		BufferHandle Buffer = BufferHandle::Invalid();
		uint64_t BufferOffset = 0;
		uint64_t BufferRange = 0;  // 0 = whole buffer
	};
}