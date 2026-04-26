// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RenderProcedure/RenderProcedure.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Graphics/Interface/ComputePipeline.hpp"
#include "Surge/Graphics/Interface/StorageBuffer.hpp"

namespace Surge
{
    class SURGE_API LightCullingProcedure : public RenderProcedure
    {
    public:
        LightCullingProcedure() = default;
        virtual ~LightCullingProcedure() = default;

        virtual void Init(RendererData* rendererData) override;
        virtual void Update() override;
        virtual void Shutdown() override;
        virtual void Resize(Uint newWidth, Uint newHeight) override;

    public:
        struct InternalData
        {
            Uint TileCountX;
            bool ShowLightComplexity = false; // Used by Renderer
            Ref<ComputePipeline> LightCullingPipeline;
            Ref<StorageBuffer> LightListStorageBuffer;
        };

    protected:
        virtual void* GetInternalDataBlock() override { return &mProcData; }

    private:
        InternalData mProcData;
        RendererData* mRendererData;
        glm::ivec2 mScreenSize;
        glm::ivec3 mLightCullingWorkGroups;

        SURGE_REFLECTION_ENABLE;
    };

} // namespace Surge
