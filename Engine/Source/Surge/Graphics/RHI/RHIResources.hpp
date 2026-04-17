// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/Core/HandlePool.hpp"
#include "Surge/Graphics/RHI/RHITypes.hpp"

// ---------------------------------------------------------------------------
// RHIResources.hpp
//   All Handle types and POD descriptor structs for the RHI layer.
//   Handles are plain {id, generation} pairs (no virtual, no RefCounted).
//   Descriptors are plain structs (no virtual, no inheritance).
// ---------------------------------------------------------------------------

namespace Surge::RHI
{
    // ── Handle Tag Types ────────────────────────────────────────────────────
    // Unique empty structs so Handle<BufferTag> and Handle<TextureTag> are
    // distinct incompatible types at the type-system level.

    struct BufferTag {};
    struct TextureTag {};
    struct SamplerTag {};
    struct ShaderTag {};
    struct GraphicsPipelineTag {};
    struct ComputePipelineTag {};
    struct RenderPassTag {};
    struct FramebufferTag {};
    struct CommandBufferTag {};
    struct SwapchainTag {};

    // ── Typed Handles ───────────────────────────────────────────────────────

    using BufferHandle           = Handle<BufferTag>;
    using TextureHandle          = Handle<TextureTag>;
    using SamplerHandle          = Handle<SamplerTag>;
    using ShaderHandle           = Handle<ShaderTag>;
    using GraphicsPipelineHandle = Handle<GraphicsPipelineTag>;
    using ComputePipelineHandle  = Handle<ComputePipelineTag>;
    using RenderPassHandle       = Handle<RenderPassTag>;
    using FramebufferHandle      = Handle<FramebufferTag>;
    using CommandBufferHandle    = Handle<CommandBufferTag>;
    using SwapchainHandle        = Handle<SwapchainTag>;

    // ── Buffer Descriptor ───────────────────────────────────────────────────

    struct BufferDesc
    {
        uint32_t       Size        = 0;
        BufferUsage    Usage       = BufferUsage::None;
        GPUMemoryUsage MemoryUsage = GPUMemoryUsage::GPUOnly;
        const void*    InitialData = nullptr; // If non-null, copied on creation
        const char*    DebugName   = nullptr;
    };

    // ── Texture Descriptor ──────────────────────────────────────────────────

    struct TextureDesc
    {
        uint32_t    Width      = 1;
        uint32_t    Height     = 1;
        uint32_t    Mips       = 1;
        uint32_t    Layers     = 1;
        ImageFormat Format     = ImageFormat::RGBA8;
        ImageUsage  Usage      = ImageUsage::Attachment;
        SamplerDesc Sampler    = {};
        const char* DebugName  = nullptr;
    };

    // ── Shader Descriptor ───────────────────────────────────────────────────

    struct ShaderDesc
    {
        const uint32_t* SpirvData   = nullptr;
        size_t          SpirvSize   = 0;    // bytes
        ShaderStage     Stage       = ShaderStage::Vertex;
        const char*     EntryPoint  = "main";
        const char*     DebugName   = nullptr;
    };

    // ── Vertex layout ───────────────────────────────────────────────────────

    struct VertexAttributeDesc
    {
        uint32_t    Location = 0;
        uint32_t    Binding  = 0;
        ImageFormat Format   = ImageFormat::RGBA32F; // VkFormat covers both
        uint32_t    Offset   = 0;
    };

    struct VertexBindingDesc
    {
        uint32_t Binding = 0;
        uint32_t Stride  = 0;
    };

    // ── Graphics Pipeline Descriptor ────────────────────────────────────────

    struct GraphicsPipelineDesc
    {
        ShaderHandle      VertexShader  = {};
        ShaderHandle      PixelShader   = {};
        PrimitiveTopology Topology      = PrimitiveTopology::TriangleList;
        PolygonMode       PolyMode      = PolygonMode::Fill;
        CullMode          Culling       = CullMode::Back;
        CompareOp         DepthCompOp   = CompareOp::Less;
        RenderPassHandle  RenderPass    = {};
        float             LineWidth     = 1.0f;
        bool              DepthTest     = true;
        bool              DepthWrite    = true;
        bool              StencilTest   = false;

        VertexAttributeDesc VertexAttributes[16]   = {};
        uint32_t            VertexAttributeCount   = 0;
        VertexBindingDesc   VertexBindings[8]      = {};
        uint32_t            VertexBindingCount     = 0;
        PushConstantRange   PushConstants[4]       = {};
        uint32_t            PushConstantCount      = 0;

        const char* DebugName = nullptr;
    };

    // ── Compute Pipeline Descriptor ─────────────────────────────────────────

    struct ComputePipelineDesc
    {
        ShaderHandle      Shader            = {};
        PushConstantRange PushConstants[4]  = {};
        uint32_t          PushConstantCount = 0;
        const char*       DebugName         = nullptr;
    };

    // ── Render Pass Attachment ───────────────────────────────────────────────
    // This describes a physical backend render pass (VkRenderPass equivalent).
    // Not to be confused with RenderGraphPass.

    struct AttachmentDesc
    {
        ImageFormat Format       = ImageFormat::None;
        LoadOp      LoadOp_      = LoadOp::Clear;
        StoreOp     StoreOp_     = StoreOp::Store;
        LoadOp      StencilLoad  = LoadOp::DontCare;
        StoreOp     StencilStore = StoreOp::DontCare;
    };

    struct RenderPassDesc
    {
        AttachmentDesc ColorAttachments[8]  = {};
        uint32_t       ColorAttachmentCount = 0;
        AttachmentDesc DepthAttachment      = {};
        bool           HasDepth             = false;
        const char*    DebugName            = nullptr;
    };

    // ── Framebuffer Descriptor ───────────────────────────────────────────────

    struct FramebufferDesc
    {
        RenderPassHandle RenderPass              = {};
        TextureHandle    ColorAttachments[8]     = {};
        uint32_t         ColorAttachmentCount    = 0;
        TextureHandle    DepthAttachment         = {};
        bool             HasDepth                = false;
        uint32_t         Width                   = 0;
        uint32_t         Height                  = 0;
        const char*      DebugName               = nullptr;
    };

    // ── Swapchain Descriptor ─────────────────────────────────────────────────

    struct SwapchainDesc
    {
        void*    WindowHandle   = nullptr; // HWND on Windows, NSWindow* on macOS
        uint32_t Width          = 1280;
        uint32_t Height         = 720;
        bool     Vsync          = false;
        uint32_t FramesInFlight = 3;
    };

} // namespace Surge::RHI
