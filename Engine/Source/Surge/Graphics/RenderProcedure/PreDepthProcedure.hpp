// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RenderProcedure/RenderProcedure.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"

namespace Surge
{
    class GraphicsPipeline;
    class Framebuffer;
    class SURGE_API PreDepthProcedure : public RenderProcedure
    {
    public:
        PreDepthProcedure() = default;
        virtual ~PreDepthProcedure() = default;

        virtual void Init(RendererData* rendererData) override;
        virtual void Update() override;
        virtual void Shutdown() override;
        virtual void Resize(Uint newWidth, Uint newHeight) override;

    public:
        struct InternalData
        {
            Ref<GraphicsPipeline> PreDepthPipeline;
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
