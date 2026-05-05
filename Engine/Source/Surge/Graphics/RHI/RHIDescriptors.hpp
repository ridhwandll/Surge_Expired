// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Graphics/RHI/RHIHandle.hpp"
#include <array>

namespace Surge
{
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
	};
	MAKE_BIT_ENUM(TextureUsage, Uint);

	enum class LoadOp { CLEAR, LOAD, DONT_CARE };
	enum class StoreOp { STORE, DONT_CARE };

	//Descriptors 

	struct BufferDesc
	{
		Uint Size = 0;
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
		const char* DebugName = nullptr;
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

	enum class ShaderType
	{
		NONE = 0,
		VERTEX,
		PIXEL,
		COMPUTE
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
		const char* VertShaderName = nullptr;
		const char* FragShaderName = nullptr;

		// Vertex layout
		std::array<VertexAttribute, 8> Attributes = {};
		Uint AttributeCount = 0;
		std::array<VertexBinding, 4> Bindings = {};
		Uint BindingCount = 0;

		// State
		RasterDesc Raster = {};
		DepthDesc  Depth = {};
		BlendDesc  Blend = {};

		// Push constants, one range, covers 99% of cases
		Uint PushConstantSize = 0; // bytes, 0 = none

		const char* DebugName = nullptr;
	};

}