// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Memory.hpp"

namespace Surge
{
    enum class SURGE_API ImageFormat
    {
        None = 0,

        RED32F,
        // Color
        RGBA8,
        RGBA16F,
        RGBA32F,

        // Depth/Stencil
        Depth32,
        Depth24Stencil8
    };

    enum class SURGE_API ImageUsage
    {
        None = 0,
        Attachment,
        Texture,
        Storage
    };

    enum class SURGE_API TextureFilter
    {
        Linear,
        Nearest
    };

    enum class SURGE_API TextureAddressMode
    {
        Repeat,
        MirroredRepeat,
        ClampToEdge,
        ClampToBorder
    };

    enum class SURGE_API CompareOp
    {
        Never,
        Less,
        Equal,
        LessOrEqual,
        Greater,
        NotEqual,
        GreaterOrEqual,
        Always,
    };

    struct SamplerProperties
    {
        bool EnableAnisotropy = false;
        bool EnableComparison = false;
        CompareOp SamplerCompareOp = CompareOp::Always;
        TextureAddressMode SamplerAddressMode = TextureAddressMode::Repeat;
        TextureFilter SamplerFilter = TextureFilter::Linear;
    };

    struct ImageSpecification
    {
        ImageFormat Format = ImageFormat::RGBA8;
        ImageUsage Usage = ImageUsage::Storage;
        SamplerProperties SamplerProps {};
        Uint Width = 0;
        Uint Height = 0;
        Uint Mips = 1;
    };

    class SURGE_API Image : public RefCounted
    {
    public:
        virtual ~Image() {}

        virtual Uint GetWidth() const = 0;
        virtual Uint GetHeight() const = 0;
        virtual void Release() = 0;
        virtual ImageSpecification& GetSpecification() = 0;
        virtual const ImageSpecification& GetSpecification() const = 0;
    };

    class SURGE_API Image2D : public Image
    {
    public:
        static Ref<Image2D> Create(const ImageSpecification& specification);
    };
} // namespace Surge
