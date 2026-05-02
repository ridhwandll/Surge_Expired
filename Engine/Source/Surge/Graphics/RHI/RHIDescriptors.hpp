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

	enum class LoadOp { CLEAR, LOAD, DONT_CARE };
	enum class StoreOp { STORE, DONT_CARE };

	//Descriptors 

	struct BufferDesc
	{
		Uint Size = 0;
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
}