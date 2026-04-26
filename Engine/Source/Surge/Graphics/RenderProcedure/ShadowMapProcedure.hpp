// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/Interface/DescriptorSet.hpp"
#include <array>
#include <glm/glm.hpp>
#include "../Renderer/Renderer.hpp"
#define MAX_CASCADE_COUNT 4

namespace Surge
{
    enum class SURGE_API CascadeCount
    {
        TWO = 2,
        THREE = 3,
        FOUR = 4
    };
    enum class SURGE_API ShadowQuality
    {
        Low = 0,
        Medium = 1,
        Ultra = 2,
        Epic = 3
    };

    FORCEINLINE Uint CascadeCountToUInt(CascadeCount c)
    {
        return static_cast<Uint>(c);
    }

    class SURGE_API ShadowMapProcedure : public RenderProcedure
    {
    public:
        ShadowMapProcedure();
        virtual ~ShadowMapProcedure() = default;

        virtual void Init(RendererData* rendererData) override;
        virtual void Update() override;
        virtual void Shutdown() override;
        virtual void Resize(Uint newWidth, Uint newHeight) override {}

        // ShadowProc functions
        const CascadeCount& GetCascadeCount() const { return mTotalCascades; };
        void SetCascadeCount(CascadeCount count);

        FORCEINLINE const ShadowQuality& GetShadowQuality() const { return mProcData.ShadowQuality; }
        FORCEINLINE void SetShadowQuality(ShadowQuality quality) { mProcData.ShadowQuality = quality; }

        FORCEINLINE const Uint& GetShadowMapsResolution() const { return mShadowMapResolution; }
        FORCEINLINE void SetShadowMapsResolution(Uint newSize)
        {
            mShadowMapResolution = newSize;
            for (Ref<Framebuffer>& framebuffer : mProcData.ShadowMapFramebuffers)
            {
                framebuffer->Resize(mShadowMapResolution, mShadowMapResolution);
            }
        }

    public:
        struct InternalData
        {
            std::array<Ref<GraphicsPipeline>, MAX_CASCADE_COUNT> ShadowMapPipelines;
            std::array<Ref<Framebuffer>, MAX_CASCADE_COUNT> ShadowMapFramebuffers;
            std::array<glm::mat4, MAX_CASCADE_COUNT> LightViewProjections = {};
            std::array<float, MAX_CASCADE_COUNT> CascadeSplitDepths = {};

            float CascadeSplitLambda = 0.91f;
            bool VisualizeCascades = false;
            Surge::ShadowQuality ShadowQuality = ShadowQuality::Ultra;

            Ref<UniformBuffer> ShadowUniformBuffer;
            Ref<DescriptorSet> ShadowDesciptorSet;
        };

    protected:
        virtual void* GetInternalDataBlock() override { return &mProcData; }

    private:
        void CalculateCascades(const glm::mat4& viewProjection, const glm::vec3& normalizedDirection);
        void UpdateShadowMapDescriptorSet();

    private:
        InternalData mProcData;
        RendererData* mRendererData;

        CascadeCount mTotalCascades;
        Uint mShadowMapResolution;
        std::array<float, MAX_CASCADE_COUNT> mCascadeSplits = {};

        SURGE_REFLECTION_ENABLE;
    };

} // namespace Surge
