// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Graphics/Interface/Image.hpp"
#include "Surge/Graphics/Interface/RenderCommandBuffer.hpp"
#include <glm/glm.hpp>

namespace Surge
{
    struct FramebufferAttachmentSpec
    {
        ImageFormat Format = {};
        SamplerProperties AttachmentSamplerProps = {};
    };

    struct FramebufferSpecification
    {
        Uint Width = 0, Height = 0;
        Vector<FramebufferAttachmentSpec> AttachmentSpecs;
        glm::vec4 ClearColor = {0.1f, 0.1f, 0.1f, 1.0f};
        bool NoResize = false;
    };

    class SURGE_API Framebuffer : public RefCounted
    {
    public:
        virtual ~Framebuffer() = default;

        virtual void Resize(Uint width, Uint height) = 0;

        virtual void BeginRenderPass(const Ref<RenderCommandBuffer>& cmdBuffer) const = 0;
        virtual void EndRenderPass(const Ref<RenderCommandBuffer>& cmdBuffer) const = 0;

        virtual const FramebufferSpecification& GetSpecification() const = 0;
        virtual const Ref<Image2D>& GetColorAttachment(Uint index) const = 0;
        virtual const Ref<Image2D>& GetDepthAttachment() const = 0;

        static Ref<Framebuffer> Create(const FramebufferSpecification& spec);
    };
} // namespace Surge
