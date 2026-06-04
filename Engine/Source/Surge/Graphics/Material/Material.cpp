// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Material.hpp"
#include "Surge/Graphics/RHI/RHI.hpp"
#include "Surge/Core/Core.hpp"

namespace Surge
{
    Material::Material(const PipelineHandle& pipeline, const Shader& shader, const String& materialBufferName)
    {
        mRHI = Core::GetRenderer()->GetRHI().get();
        mRefletedBuffer = shader.GetReflectionData().GetBuffer(materialBufferName);
        mReflectedResources = shader.GetReflectionData().GetResources();

        mBufferBinding = mRefletedBuffer.Binding;
        mCPUData.Allocate(mRefletedBuffer.Size);
        mMaterialDescriptorSlot = static_cast<DescriptorSetSlot>(mRefletedBuffer.Set);

        mDescriptorSet = mRHI->CreateDescriptorSet(pipeline, mMaterialDescriptorSlot, DescriptorUpdateFrequency::DYNAMIC, materialBufferName.c_str());

        BufferDesc bufferDesc = {};
        bufferDesc.Size = mRefletedBuffer.Size;
        bufferDesc.InitialData = nullptr;
        bufferDesc.Usage = BufferUsage::UNIFORM;
        bufferDesc.HostVisible = true;
        bufferDesc.DebugName = materialBufferName;

        // (RID) Instead of creating RHISettings::FRAMES_IN_FLIGHT amount of VkBuffer for each material, we could have a
        // global giant VkBuffer for each shader buffer block, and suballocate from that for each material. This would
        // reduce memory fragmentation and the number of buffers we have to manage, but it would add complexity to buffer
        // management and synchronization. For now we will stick with the simpler approach of one buffer per material per frame

        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
            mGPUBuffers[i] = mRHI->CreateBuffer(bufferDesc);

        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
        {
            DescriptorWrite write = {};
            write.Binding = mBufferBinding;
            write.Buffer = mGPUBuffers[i];
            write.Type = DescriptorType::UNIFORM_BUFFER;
            write.BufferRange = mRefletedBuffer.Size;
            mRHI->UpdateDescriptorSet(mDescriptorSet, &write, 1, i);
        }
        MarkDirty();
    }

    Material::~Material()
    {
        //mRHI->WaitIdle();

        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
            mRHI->DestroyBuffer(mGPUBuffers[i]);

        mRHI->DestroyDescriptorSet(mDescriptorSet);
    }

    void Material::Bind(const FrameContext& ctx, PipelineHandle pipeline) const
    {
        UpdateForRendering(ctx);
        mRHI->CmdBindDescriptorSet(ctx, pipeline, mDescriptorSet, mMaterialDescriptorSlot);
    }

    void Material::UpdateForRendering(const FrameContext& ctx) const
    {
        if (mIsDirty[ctx.FrameIndex])
        {
            mRHI->UploadBuffer(mGPUBuffers[ctx.FrameIndex], mCPUData.As<void>(), mCPUData.GetSize());
            mIsDirty[ctx.FrameIndex] = false;
        }
    }
}
