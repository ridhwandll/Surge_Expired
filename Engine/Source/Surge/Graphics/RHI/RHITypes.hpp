// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"

// ---------------------------------------------------------------------------
// RHITypes.hpp
//   All primitive enums and plain structs shared across the RHI layer.
//   Lives in Surge::RHI so it does not clash with the legacy Surge:: types
//   that remain in Interface/ during the migration period.
// ---------------------------------------------------------------------------

namespace Surge::RHI
{
    // ── Image / Texture ──────────────────────────────────────────────────────

    enum class ImageFormat : uint8_t
    {
        None = 0,

        // Colour
        R8,
        R16F,
        R32F,
        RG8,
        RG16F,
        RG32F,
        RGBA8,
        BGRA8,   // Swapchain format
        RGBA16F,
        RGBA32F,

        // Depth / Stencil
        Depth16,
        Depth32F,
        Depth24Stencil8
    };

    enum class ImageUsage : uint8_t
    {
        None        = 0,
        Attachment  = BIT(0), // render target / depth-stencil
        Sampled     = BIT(1), // read in shaders
        Storage     = BIT(2), // read+write in shaders (UAV / StorageImage)
        TransferSrc = BIT(3),
        TransferDst = BIT(4)
    };
    MAKE_BIT_ENUM(ImageUsage)

    enum class TextureFilter    : uint8_t { Nearest, Linear };
    enum class TextureAddressMode : uint8_t { Repeat, MirroredRepeat, ClampToEdge, ClampToBorder };
    enum class TextureMipmapMode  : uint8_t { Nearest, Linear };

    enum class CompareOp : uint8_t
    {
        Never,
        Less,
        Equal,
        LessOrEqual,
        Greater,
        NotEqual,
        GreaterOrEqual,
        Always
    };

    struct SamplerDesc
    {
        TextureFilter      MinFilter        = TextureFilter::Linear;
        TextureFilter      MagFilter        = TextureFilter::Linear;
        TextureMipmapMode  MipmapMode       = TextureMipmapMode::Linear;
        TextureAddressMode AddressModeU     = TextureAddressMode::Repeat;
        TextureAddressMode AddressModeV     = TextureAddressMode::Repeat;
        TextureAddressMode AddressModeW     = TextureAddressMode::Repeat;
        float              MaxAnisotropy    = 1.0f;
        float              MipLodBias       = 0.0f;
        float              MinLod           = 0.0f;
        float              MaxLod           = 1.0f;
        bool               EnableAnisotropy = false;
        bool               EnableCompare    = false;
        CompareOp          SamplerCompareOp = CompareOp::Always;
    };

    // ── Buffer ──────────────────────────────────────────────────────────────

    enum class BufferUsage : uint16_t
    {
        None           = 0,
        VertexBuffer   = BIT(0),
        IndexBuffer    = BIT(1),
        UniformBuffer  = BIT(2),
        StorageBuffer  = BIT(3),
        TransferSrc    = BIT(4),
        TransferDst    = BIT(5),
        IndirectBuffer = BIT(6)
    };
    MAKE_BIT_ENUM(BufferUsage)

    enum class GPUMemoryUsage : uint8_t
    {
        Unknown = 0,
        GPUOnly,
        CPUOnly,
        CPUToGPU,  // Upload heap
        GPUToCPU,  // Readback heap
        CPUCopy,
        GPULazilyAllocated
    };

    // ── Pipeline ────────────────────────────────────────────────────────────

    enum class PrimitiveTopology : uint8_t
    {
        PointList,
        LineList,
        LineStrip,
        TriangleList,
        TriangleStrip
    };

    enum class PolygonMode : uint8_t { Fill, Line, Point };
    enum class CullMode    : uint8_t { None, Front, Back, FrontAndBack };

    // ── Shader stages ───────────────────────────────────────────────────────

    enum class ShaderStage : uint8_t
    {
        None    = 0,
        Vertex  = BIT(0),
        Pixel   = BIT(1), // Fragment
        Compute = BIT(2),
        All     = Vertex | Pixel | Compute
    };
    MAKE_BIT_ENUM(ShaderStage)

    // ── Queue ────────────────────────────────────────────────────────────────

    enum class QueueType : uint8_t { Graphics, Compute, Transfer };

    // ── Render pass attachment ops ──────────────────────────────────────────

    enum class LoadOp  : uint8_t { Load, Clear, DontCare };
    enum class StoreOp : uint8_t { Store, DontCare };

    // ── Viewport / Scissor ──────────────────────────────────────────────────

    struct Viewport
    {
        float x        = 0.0f;
        float y        = 0.0f;
        float Width    = 0.0f;
        float Height   = 0.0f;
        float MinDepth = 0.0f;
        float MaxDepth = 1.0f;
    };

    struct Rect2D
    {
        int32_t  X      = 0;
        int32_t  Y      = 0;
        uint32_t Width  = 0;
        uint32_t Height = 0;
    };

    // ── Clear values ────────────────────────────────────────────────────────

    union ClearColorValue
    {
        float    Float32[4]  = {0.0f, 0.0f, 0.0f, 1.0f};
        int32_t  Int32[4];
        uint32_t UInt32[4];
    };

    struct ClearDepthStencilValue
    {
        float    Depth   = 1.0f;
        uint32_t Stencil = 0;
    };

    struct ClearValue
    {
        bool                   IsDepth      = false;
        ClearColorValue        Color        = {};
        ClearDepthStencilValue DepthStencil = {};
    };

    // ── Push constants ──────────────────────────────────────────────────────

    struct PushConstantRange
    {
        ShaderStage Stages = ShaderStage::All;
        uint32_t    Offset = 0;
        uint32_t    Size   = 0;
    };

} // namespace Surge::RHI
