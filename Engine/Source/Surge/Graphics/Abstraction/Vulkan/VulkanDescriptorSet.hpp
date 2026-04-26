// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/Interface/DescriptorSet.hpp"
#include <volk/volk.h>

namespace Surge
{
    class RenderCommandBuffer;
    class SURGE_API VulkanDescriptorSet : public DescriptorSet
    {
    public:
        VulkanDescriptorSet(const Ref<Shader>& shader, Uint setNumber, bool resetEveryFrame, int index = -1);
        ~VulkanDescriptorSet();

        virtual void Bind(const Ref<RenderCommandBuffer>& commandBuffer, const Ref<GraphicsPipeline>& pipeline) override;
        virtual void Bind(const Ref<RenderCommandBuffer>& commandBuffer, const Ref<ComputePipeline>& pipeline) override;

        virtual void UpdateForRendering() override;
        virtual void SetBuffer(const Ref<UniformBuffer>& dataBuffer, Uint binding) override { mPendingBuffers.push_back({binding, dataBuffer}); }
        virtual void SetBuffer(const Ref<StorageBuffer>& dataBuffer, Uint binding) override { mPendingStorageBuffers.push_back({binding, dataBuffer}); }
        virtual void SetImage2D(const Ref<Image2D>& image, Uint binding) override { mPendingImages.push_back({binding, image}); }

        Vector<VkDescriptorSet> GetVulkanDescriptorSets() { return mDescriptorSets; }

    private:
        Uint mSetNumber;
        Vector<VkDescriptorSet> mDescriptorSets;

        Vector<Pair<Uint, Ref<StorageBuffer>>> mPendingStorageBuffers;
        Vector<Pair<Uint, Ref<UniformBuffer>>> mPendingBuffers;
        Vector<Pair<Uint, Ref<Image2D>>> mPendingImages;
    };

} // namespace Surge
