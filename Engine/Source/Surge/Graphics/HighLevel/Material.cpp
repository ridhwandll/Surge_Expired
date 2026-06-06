// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Material.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Graphics/RHI/RHI.hpp"
#include "Texture2D.hpp"

#undef FindResource // FUCK ASS WIN32

namespace Surge
{
    Material::Material(const PipelineHandle& pipeline, const Shader& shader, const String& materialBufferName)
    {
        mShaderName = shader.GetName();
        Initialize(pipeline, shader, materialBufferName);
    }

    Material::~Material()
    {
        //mRHI->WaitIdle();

        for(Uint i = 0; i < RHISettings::FRAMES_IN_FLIGHT; i++)
            mRHI->DestroyBuffer(mGPUBuffers[i]);

        for(const auto& [name, tex] : mTextures)
        {
            if(tex.Data1) // If we have a Ref<Texture2D>, we own the texture and should destroy it. If we only have an ImageHandle, we don't own it and shouldn't destroy it
                mRHI->DestroyImage(tex.Data2);
        }

        mRHI->DestroyDescriptorSet(mDescriptorSet);
    }

    void Material::Bind(const FrameContext& ctx, PipelineHandle pipeline) const
    {
        UpdateForRendering(ctx);
        mRHI->CmdBindDescriptorSet(ctx, pipeline, mDescriptorSet, mMaterialDescriptorSlot);
    }

    const Ref<Texture2D>& Material::GetTexture(const String& name)
    {
        const ShaderResource* resource = FindResource(name);
        SG_ASSERT(resource, "Material::GetTexture: '{}' is not a reflected texture name!", name);
        return mTextures[name].Data1;
    }

    void Material::SetTexture(const String& name, const Ref<Texture2D>& texture)
    {
        //SG_ASSERT(texture, "Material::SetTexture: texture is null!");

        const ShaderResource* resource = FindResource(name);
        SG_ASSERT(resource, "Material::SetTexture: '{}' is not a reflected texture name!", name);

        texture ?
        mTextures[name] = Pair<Ref<Texture2D>, ImageHandle>(texture, texture->GetRHIImage()) :
        mTextures[name] = Pair<Ref<Texture2D>, ImageHandle>(nullptr, mWhiteTexture);

        MarkDirty();
    }

    void Material::SetTexture(const String& name, ImageHandle handle)
    {

        const ShaderResource* resource = FindResource(name);
        SG_ASSERT(resource, "Material::SetTexture: '{}' is not a reflected texture name!", name);

        mTextures[name] = Pair<Ref<Texture2D>, ImageHandle>(nullptr, handle);
        MarkDirty();
    }

    void Material::UpdateForRendering(const FrameContext& ctx) const
    {
        if(!mIsDirty[ctx.FrameIndex])
            return;

        Vector<DescriptorWrite> writes;
        writes.reserve(mTextures.size() + 1);

        DescriptorWrite bufferWrite = {};
        bufferWrite.Binding = mBufferBinding;
        bufferWrite.Type = DescriptorType::UNIFORM_BUFFER;
        bufferWrite.Buffer = mGPUBuffers[ctx.FrameIndex];
        bufferWrite.BufferRange = mRefletedBuffer.Size;
        writes.push_back(bufferWrite);

        // Textures driven by reflection, no hardcoded bindings
        for(const auto& [name, tex] : mTextures)
        {
            const ShaderResource* resource = FindResource(name);
            SG_ASSERT(resource, "Material::UpdateForRendering: '{}' is not a reflected texture name!", name);

            DescriptorWrite write = {};
            write.Binding = resource->Binding;
            write.Type = DescriptorType::TEXTURE;
            write.Texture = tex.Data2; // tex.Data2 is always a valid ImageHandle, even if we don't own the texture. If we have a Ref<Texture2D>, we get the ImageHandle from it. If we only have an ImageHandle, we use that directly
            write.Sampler = Core::GetRenderer()->GetDefaultSampler();
            writes.push_back(write);
        }

        mRHI->UploadBuffer(mGPUBuffers[ctx.FrameIndex], mCPUData.As<void>(), mCPUData.GetSize());
        mRHI->UpdateDescriptorSet(mDescriptorSet, writes.data(), (Uint)writes.size(), ctx.FrameIndex);
        mIsDirty[ctx.FrameIndex] = false;
    }

    Ref<Material> Material::Create()
    {
        // TODO: NOT HARDCODE Pipelines and shader names!
        Renderer* r = Core::GetRenderer();
        Ref<Material> material = Ref<Material>::Create(r->GetRenderGraphBlackBoard().MaterialPipeline, r->GetShaderManager().Get("Renderer3D.glsl"), "Material");
        return material;
    }

    void Material::Initialize(const PipelineHandle& pipeline, const Shader& shader, const String& materialBufferName)
    {
        Renderer* renderer = Core::GetRenderer();
        mRHI = renderer->GetRHI().get();
        mRefletedBuffer = shader.GetReflectionData().GetBuffer(materialBufferName);
        mReflectedResources = shader.GetReflectionData().GetResources();

        mBufferBinding = mRefletedBuffer.Binding;
        mCPUData.Allocate(mRefletedBuffer.Size);
        mCPUData.ZeroInitialize();
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

        mWhiteTexture = renderer->GetWhiteTexture();
        SetTexture("AlbedoMap", mWhiteTexture);
        SetTexture("NormalMap", mWhiteTexture);
        SetTexture("RoughnessMetallicMap", mWhiteTexture);
    }

    const Surge::ShaderResource* Material::FindResource(const String& name) const
    {
        for(const ShaderResource& resource : mReflectedResources)
        {
            if(resource.Name == name)
                return &resource;
        }
        return nullptr;
    }

}
