// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RHI/RHIDescs.hpp"
#include <volk.h>

namespace Surge
{
    // Key that uniquely identifies a VkRenderPass configuration
    // Two framebuffers with same formats + load/store ops share one VkRenderPass
    struct RenderPassKey
    {
        std::array<VkFormat, 8> ColorFormats = {};
        Uint ColorCount = 0;
        VkFormat DepthFormat = VK_FORMAT_UNDEFINED;

        std::array<LoadOp, 8> ColorLoad = {};
        std::array<StoreOp, 8> ColorStore = {};
        LoadOp DepthLoad = LoadOp::CLEAR;
        StoreOp DepthStore = StoreOp::STORE;

        bool operator==(const RenderPassKey& o) const
        {
            if (ColorCount != o.ColorCount)   return false;
            if (DepthFormat != o.DepthFormat) return false;

            for (Uint i = 0; i < ColorCount; i++)
            {
                if (ColorFormats[i] != o.ColorFormats[i]) return false;
                if (ColorLoad[i] != o.ColorLoad[i])       return false;
                if (ColorStore[i] != o.ColorStore[i])     return false;
            }
            return DepthLoad == o.DepthLoad && DepthStore == o.DepthStore;
        }
    };

    struct RenderPassKeyHash
    {
        size_t operator()(const RenderPassKey& k) const
        {
            size_t hash = 0;
            auto combine = [&](size_t v)
                {
                    hash ^= v + 0x9e3779b9 + (hash << 6) + (hash >> 2);
                };

            for (Uint i = 0; i < k.ColorCount; i++)
            {
                combine(std::hash<int>{}(k.ColorFormats[i]));
                combine(std::hash<int>{}((int)k.ColorLoad[i]));
                combine(std::hash<int>{}((int)k.ColorStore[i]));
            }
            combine(std::hash<int>{}(k.DepthFormat));
            combine(std::hash<int>{}((int)k.DepthLoad));
            combine(std::hash<int>{}((int)k.DepthStore));
            return hash;
        }
    };

    class VulkanRHI;
    class VulkanRenderpassFactory
    {
    public:
        // Returns existing or creates new VkRenderPass for the given key
        VkRenderPass GetOrCreate(const VulkanRHI& rhi, const RenderPassKey& key);
        void Shutdown(const VulkanRHI& ctx);

    private:
        VkRenderPass Create(const VulkanRHI& rhi, const RenderPassKey& key);
        std::unordered_map<RenderPassKey, VkRenderPass, RenderPassKeyHash> mCache;
    };
}