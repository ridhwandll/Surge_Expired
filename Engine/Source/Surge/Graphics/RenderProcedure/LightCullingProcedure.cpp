// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/RenderProcedure/LightCullingProcedure.hpp"
#include "PreDepthProcedure.hpp"
#include "GeometryProcedure.hpp"

namespace Surge
{
    void LightCullingProcedure::Init(RendererData* rendererData)
    {
        mRendererData = rendererData;
        Ref<Shader>& lightCullingShader = mRendererData->ShaderSet.GetShader("LightCulling");
        mProcData.LightCullingPipeline = ComputePipeline::Create(lightCullingShader);

        mProcData.LightListStorageBuffer = StorageBuffer::Create(1, GPUMemoryUsage::GPUToCPU); // Size of `1`, resized later. GPU will write to this buffer
    }

    void LightCullingProcedure::Update()
    {
        const Ref<Image2D>& preDepthImage = Core::GetRenderer()->GetRenderProcManager()->GetRenderProcData<PreDepthProcedure>()->OutputFrambuffer->GetDepthAttachment();
        Ref<RenderCommandBuffer>& cmd = mRendererData->RenderCmdBuffer;
        mProcData.LightCullingPipeline->Bind(cmd);

        mRendererData->LightData.CameraPosition = mRendererData->CameraPosition;
        mRendererData->LightData.PointLightCount = Uint(mRendererData->PointLights.size());
        for (Uint i = 0; i < mRendererData->LightData.PointLightCount; i++)
            mRendererData->LightData.PointLights[i] = mRendererData->PointLights[i];
        mRendererData->LightData.DirLight = mRendererData->DirLight;
        mRendererData->LightUniformBuffer->SetData(&mRendererData->LightData);

        mRendererData->DescriptorSet0->SetBuffer(mRendererData->LightUniformBuffer, 2);
        mRendererData->DescriptorSet0->SetBuffer(mProcData.LightListStorageBuffer, 3);
        mRendererData->DescriptorSet0->SetImage2D(preDepthImage, 4);
        mRendererData->DescriptorSet0->UpdateForRendering();
        mRendererData->DescriptorSet0->Bind(cmd, mProcData.LightCullingPipeline);

        mProcData.LightCullingPipeline->SetPushConstantData(cmd, "uScreenData", &mScreenSize);
        mProcData.LightCullingPipeline->Dispatch(cmd, mLightCullingWorkGroups.x, mLightCullingWorkGroups.y, mLightCullingWorkGroups.z);
    }

    void LightCullingProcedure::Resize(Uint newWidth, Uint newHeight)
    {
        mLightCullingWorkGroups = {(newWidth + newWidth % 16) / 16, (newHeight + newHeight % 16) / 16, 1};
        mProcData.TileCountX = mLightCullingWorkGroups.x;
        int newBufferSize = mLightCullingWorkGroups.x * mLightCullingWorkGroups.y * 4096;
        mProcData.LightListStorageBuffer->Resize(newBufferSize);
        mScreenSize = {newWidth, newHeight};
    }

    void LightCullingProcedure::Shutdown()
    {
        mProcData.LightCullingPipeline.Reset();
        mProcData.LightListStorageBuffer.Reset();
    }

} // namespace Surge

SURGE_REFLECT_CLASS_REGISTER_BEGIN(Surge::LightCullingProcedure)
SURGE_REFLECT_CLASS_REGISTER_END(Surge::LightCullingProcedure)