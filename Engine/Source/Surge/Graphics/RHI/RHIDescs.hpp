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
        VERTEX,
        INDEX,
        UNIFORM,
        STORAGE,
    };

    enum class ImageFormat
    {
        R8_UNORM,
        RGBA8_SRGB,
        RGBA8_UNORM,
        BGRA8_SRGB,
        D32_SFLOAT,
        D16_UNORM,
        D24_UNORM_S8_UINT,
        R16G16B16A16_SFLOAT,
        B10G11R11_UFLOAT_PACK32,

        ASTC4x4_SRGB,
        ASTC4x4_UNORM,
        BC7_SRGB,
        BC7_UNORM
    };

    enum class ImageUsage : Uint
    {
        SAMPLED = BIT(0),
        COLOR_ATTACHMENT = BIT(1),
        DEPTH_ATTACHMENT = BIT(2),
        TRANSIENT_ATTACHMENT = BIT(3),
        STORAGE = BIT(4),
        TRANSFER_SRC = BIT(5),
        TRANSFER_DST = BIT(6)
    };
    MAKE_BIT_ENUM(ImageUsage, Uint);

    enum class LoadOp { CLEAR, LOAD, DONT_CARE };
    enum class StoreOp { STORE, DONT_CARE };

    //Descriptors

    struct BufferDesc
    {
        uint64_t Size = 0;
        const void* InitialData = nullptr;
        BufferUsage Usage = BufferUsage::VERTEX;
        bool HostVisible = false;
        String DebugName;
    };

    struct MipUploadData
    {
        const void* Data = nullptr;
        Uint Size = 0; // bytes for this level
        Uint Width = 0;
        Uint Height = 0;
    };
    struct ImageDesc
    {
        Uint Width = 1;
        Uint Height = 1;
        Uint MipLevel = 1;
        Uint Layers = 1;
        ImageFormat Format = ImageFormat::RGBA8_SRGB;
        ImageUsage  Usage = ImageUsage::SAMPLED;
        bool Transient = false;
        SamplerHandle Sampler = {};

        // Level 0 only, RHI generates remaining mips via vkCmdBlitImage if MipLevel > 1 and GenerateMips is true
        // This is not supported for compressed formats, they must provide all levels via MipUploads
        const void* InitialData = nullptr;
        Uint DataSize = 0;

        // If set, InitialData/DataSize are ignored
        // MipLevel must match MipUploadCount
        const MipUploadData* MipUploads = nullptr;
        Uint MipUploadCount = 0;

        bool GenerateImGuiID = false;
        String DebugName;
    };

    struct FramebufferAttachment
    {
        ImageHandle Handle;
        LoadOp Load = LoadOp::CLEAR;
        StoreOp Store = StoreOp::STORE;
        LoadOp StencilLoad = LoadOp::DONT_CARE;
        StoreOp StencilStore = StoreOp::DONT_CARE;
    };

    struct FramebufferDesc
    {
        std::array<FramebufferAttachment, 8> ColorAttachments = {};
        Uint ColorAttachmentCount = 0;

        FramebufferAttachment DepthAttachment = {};
        bool HasDepth = false;
        Uint Width = 0;
        Uint Height = 0;

        String DebugName;
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

        bool DepthBiasEnable = false;
        float DepthBiasConstantFactor = 1.25f;
        float DepthBiasSlopeFactor = 1.75f;
        float DepthBiasClamp = 0.0f;
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

    enum class StencilOp { KEEP, ZERO, REPLACE, INCREMENT_AND_CLAMP, DECREMENT_AND_CLAMP };
    struct StencilOpState
    {
        StencilOp Fail = StencilOp::KEEP;
        StencilOp Pass = StencilOp::KEEP;
        StencilOp DepthFail = StencilOp::KEEP;
        CompareOp CompareOp_ = CompareOp::ALWAYS;
        uint32_t Reference = 0;
        uint32_t WriteMask = 0xFF;
        uint32_t CompareMask = 0xFF;
    };

    struct StencilDesc
    {
        bool Enable = false;
        StencilOpState Front;
        StencilOpState Back;
    };

    struct PipelineDesc
    {
        // Attributes and bindings are reflected from shader via SPIRV-Cross

        // Shaders
        Shader Shader_; 

        // State
        RasterDesc Raster = {};
        DepthDesc Depth = {};
        BlendDesc Blend = {};
        StencilDesc Stencil = {};

        FramebufferHandle TargetFramebuffer = FramebufferHandle::Invalid();
        bool TargetSwapchain = false; // Swapchain pass

        String DebugName = "";
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

        bool CompareEnable = false;
        CompareOp CompareOp_ = CompareOp::LESS_OR_EQUAL;

        String DebugName;
    };

    enum class DescriptorType : Uint
    {
        TEXTURE,         // combined image + sampler
        STORAGE_TEXTURE, // read/write image
        UNIFORM_BUFFER,  // small read-only buffer
        STORAGE_BUFFER,  // large read/write buffer
        SAMPLER,         // separate sampler
    };

    enum class DescriptorUpdateFrequency
    {
        STATIC,  // Set once, never updated, skybox, font atlas, LUTs
        DYNAMIC, // Updated per frame. per-object params, animated materials
    };

    struct DescriptorWrite
    {
        Uint Binding = 0;
        Uint ArrayIndex = 0; // for array bindings
        DescriptorType Type = DescriptorType::TEXTURE;

        ImageHandle Texture = ImageHandle::Invalid();
        SamplerHandle Sampler = SamplerHandle::Invalid();
        BufferHandle Buffer = BufferHandle::Invalid();
        uint64_t BufferOffset = 0;
        uint64_t BufferRange = 0; // 0 = whole buffer
    };

    enum DescriptorSetSlot : Uint
    {
        ZERO = 0,
        ONE = 1,
        TWO = 2,
        THREE = 3,
    };
}