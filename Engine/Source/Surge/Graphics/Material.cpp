// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Material.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanRHIDevice.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanRHISwapchain.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanShader.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanImage.hpp"
#include "Surge/Core/Core.hpp"
#include <vk_mem_alloc.h>

#define MATERIAL_SET 1

namespace Surge
{
    Ref<Texture2D> Material::mDummyTexture = nullptr;

    Ref<Material> Material::Create(const String& shaderName, const String& materialName)
    {
        return Ref<Material>::Create(Core::GetRenderer()->GetShader(shaderName), materialName);
    }

    Material::Material(const Ref<Shader>& shader, const String& materialName)
    {
        mName = materialName;
        mShader = shader;
        mShaderReloadID = mShader->AddReloadCallback([&]() { Load(); });

        const ShaderReflectionData& reflectionData = mShader->GetReflectionData();
        mShaderBuffer = reflectionData.GetBuffer("Material");
        mBufferMemory.Allocate(mShaderBuffer.Size);
        mBufferMemory.ZeroInitialize();

        Load();
    }

    Material::~Material()
    {
        Release();
        mShader->RemoveReloadCallback(mShaderReloadID);
        mBufferMemory.Release();
    }

    void Material::Load()
    {
        Release();

        Ref<VulkanShader> vulkanShader = mShader.As<VulkanShader>();
        VkDevice device = RHI::gVulkanRHIDevice.GetLogicalDevice();

        Uint set = 0;
        const Vector<ShaderBuffer>& shaderBuffers = vulkanShader->GetReflectionData().GetBuffers();
        for (auto& shaderBuffer : shaderBuffers)
        {
            if (shaderBuffer.Set == MATERIAL_SET)
            {
                mBinding = shaderBuffer.Binding;
                set = shaderBuffer.Set;
                break;
            }
        }

        const ShaderReflectionData& reflectionData = mShader->GetReflectionData();
        const Vector<ShaderResource>& ress = reflectionData.GetResources();
        for (const ShaderResource& res : ress)
        {
            if (res.Set == 2)
                mShaderResources[res.Binding] = res;
        }
        const Ref<Texture2D>& whiteTex = Core::GetRenderer()->GetWhiteTexture();
        for (auto& [binding, res] : mShaderResources)
        {
            mTextures[binding] = whiteTex;
            mUpdatePendingTextures.push_back({binding, mTextures.at(binding).Raw()});
        }

        // Create uniform buffer via VMA
        VkBufferCreateInfo bufInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufInfo.size = mShaderBuffer.Size;
        bufInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VmaAllocation vmaAlloc = nullptr;
        vmaCreateBuffer(RHI::gVulkanRHIDevice.GetVMAAllocator(), &bufInfo, &allocInfo, &mVkUniformBuffer, &vmaAlloc, nullptr);
        mAllocation = vmaAlloc;

        mUniformDescriptorInfo.buffer = mVkUniformBuffer;
        mUniformDescriptorInfo.range = mShaderBuffer.Size;
        mUniformDescriptorInfo.offset = 0;

        mDescriptorSets.resize(RHI::FRAMES_IN_FLIGHT);
        for (Uint i = 0; i < mDescriptorSets.size(); i++)
        {
            VkDescriptorSetAllocateInfo dsAlloc {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            dsAlloc.descriptorSetCount = 1;
            dsAlloc.pSetLayouts = &vulkanShader->GetDescriptorSetLayouts().at(set);
            dsAlloc.descriptorPool = RHI::gVulkanRHIDevice.GetPersistentDescriptorPool(i);
            vkAllocateDescriptorSets(device, &dsAlloc, &mDescriptorSets[i]);
        }
        mTextureDescriptorSets.resize(RHI::FRAMES_IN_FLIGHT);
        for (Uint i = 0; i < mTextureDescriptorSets.size(); i++)
        {
            VkDescriptorSetAllocateInfo dsAlloc {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            dsAlloc.descriptorSetCount = 1;
            dsAlloc.pSetLayouts = &vulkanShader->GetDescriptorSetLayouts().at(2);
            dsAlloc.descriptorPool = RHI::gVulkanRHIDevice.GetPersistentDescriptorPool(i);
            vkAllocateDescriptorSets(device, &dsAlloc, &mTextureDescriptorSets[i]);
        }
        UpdateForRendering();
    }

    void Material::UpdateForRendering()
    {
        SURGE_PROFILE_FUNC("Material::UpdateForRendering");
        VkDevice logicalDevice = RHI::gVulkanRHIDevice.GetLogicalDevice();
        uint32_t frameIndex = RHI::vkRHISwapchainGetFrameIndex(Core::GetSwapchain());

        Vector<VkWriteDescriptorSet> writeDescriptorSets;
        if (!mUpdatePendingTextures.empty())
        {
            for (auto& [binding, texture] : mUpdatePendingTextures)
            {
                for (Uint fidx = 0; fidx < RHI::FRAMES_IN_FLIGHT; ++fidx)
                {
                    VkWriteDescriptorSet& textureWriteDescriptorSet = writeDescriptorSets.emplace_back();
                    textureWriteDescriptorSet = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                    textureWriteDescriptorSet.dstBinding = binding;
                    textureWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    textureWriteDescriptorSet.pImageInfo = &texture->GetImage2D().As<VulkanImage2D>()->GetVulkanDescriptorImageInfo();
                    textureWriteDescriptorSet.descriptorCount = 1;
                    textureWriteDescriptorSet.dstSet = mTextureDescriptorSets[fidx];
                }
                vkUpdateDescriptorSets(logicalDevice, (Uint)writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);
                writeDescriptorSets.clear();
            }
            mUpdatePendingTextures.clear();
        }

        // Upload uniform buffer data
        VmaAllocation vmaAlloc = (VmaAllocation)mAllocation;
        VmaAllocationInfo vmaInfo = {};
        vmaGetAllocationInfo(RHI::gVulkanRHIDevice.GetVMAAllocator(), vmaAlloc, &vmaInfo);
        if (vmaInfo.pMappedData && mBufferMemory.Size > 0)
            memcpy(vmaInfo.pMappedData, mBufferMemory.Data, mBufferMemory.Size);

        VkWriteDescriptorSet bufferWrite = {};
        bufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        bufferWrite.dstBinding = mBinding;
        bufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bufferWrite.pBufferInfo = &mUniformDescriptorInfo;
        bufferWrite.descriptorCount = 1;
        bufferWrite.dstSet = mDescriptorSets[frameIndex];
        vkUpdateDescriptorSets(logicalDevice, 1, &bufferWrite, 0, nullptr);
    }

    void Material::Bind(VkCommandBuffer cmd, VkPipelineLayout layout, uint32_t frameIndex) const
    {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 1, 1, &mDescriptorSets[frameIndex], 0, nullptr);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 2, 1, &mTextureDescriptorSets[frameIndex], 0, nullptr);
    }

    void Material::Release()
    {
        if (mVkUniformBuffer == VK_NULL_HANDLE)
            return;

        VkDevice logicalDevice = RHI::gVulkanRHIDevice.GetLogicalDevice();
        vkDeviceWaitIdle(logicalDevice);

        for (Uint i = 0; i < mDescriptorSets.size(); i++)
        {
            if (mDescriptorSets[i] != VK_NULL_HANDLE)
            {
                vkFreeDescriptorSets(logicalDevice, RHI::gVulkanRHIDevice.GetPersistentDescriptorPool(i), 1, &mDescriptorSets[i]);
                mDescriptorSets[i] = VK_NULL_HANDLE;
            }
        }
        for (Uint i = 0; i < mTextureDescriptorSets.size(); i++)
        {
            if (mTextureDescriptorSets[i] != VK_NULL_HANDLE)
            {
                vkFreeDescriptorSets(logicalDevice, RHI::gVulkanRHIDevice.GetPersistentDescriptorPool(i), 1, &mTextureDescriptorSets[i]);
                mTextureDescriptorSets[i] = VK_NULL_HANDLE;
            }
        }

        vmaDestroyBuffer(RHI::gVulkanRHIDevice.GetVMAAllocator(), mVkUniformBuffer, (VmaAllocation)mAllocation);
        mVkUniformBuffer = VK_NULL_HANDLE;
        mAllocation = nullptr;
    }

    void Material::RemoveTexture(const String& name)
    {
        const Ref<Texture2D>& whiteTex = Core::GetRenderer()->GetWhiteTexture();
        this->Set<Ref<Texture2D>>(name, whiteTex);
    }

} // namespace Surge