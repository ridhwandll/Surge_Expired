// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RenderProcedure/RenderProcedure.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Graphics/Interface/Framebuffer.hpp"
#include "Surge/Graphics/Interface/GraphicsPipeline.hpp"

namespace Surge
{
    class SURGE_API GeometryProcedure : public RenderProcedure
    {
    public:
        GeometryProcedure() = default;
        virtual ~GeometryProcedure() = default;

        virtual void Init(RendererData* rendererData) override;
        virtual void Update() override;
        virtual void Shutdown() override;
        virtual void Resize(Uint newWidth, Uint newHeight) override;

    public:
        struct InternalData
        {
            Ref<GraphicsPipeline> GeometryPipeline;
            Ref<Framebuffer> OutputFrambuffer;
        };

    protected:
        virtual void* GetInternalDataBlock() override { return &mProcData; }

    private:
        InternalData mProcData;
        RendererData* mRendererData;

        SURGE_REFLECTION_ENABLE;
    };

} // namespace Surge
