// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/Vector.hpp"
#include "Surge/Graphics/RHI/RHIDescs.hpp"
#include "Surge/Graphics/RHI/RHISettings.hpp"
#include <volk.h>
#include <array>
#include <string_view>

namespace Surge
{
    class VulkanRHI;

    // GPU timestamp queries: one query range per frame slot, nested named scopes written with vkCmdWriteTimestamp (BOTTOM_OF_PIPE, so
    // sibling scopes split the timeline instead of overlapping). A slot's results are read right after its fence wait, without blocking,
    // so they are FRAMES_IN_FLIGHT frames old. Enable/disable requests apply at the next BeginFrame, a frame is always timed fully or not at all
    class VulkanGPUTimer
    {
    public:
        void Initialize(const VulkanRHI& rhi, bool enabled);
        void Shutdown(const VulkanRHI& rhi);

        void Collect(const VulkanRHI& rhi, Uint frameIndex);      // After the slot's fence wait
        void BeginFrame(VkCommandBuffer cmd, Uint frameIndex);    // Right after vkBeginCommandBuffer (outside of any render pass)
        void EndFrame(VkCommandBuffer cmd);                       // Right before vkEndCommandBuffer, closes scopes left open

        // wait: the scope times the GPU idling (not work), Collect() reports it in DisplayWaitMilliseconds and takes it off its parents
        void BeginScope(VkCommandBuffer cmd, std::string_view name, bool wait = false);
        void EndScope(VkCommandBuffer cmd);

        void SetEnabled(bool enabled) { mRequestedEnabled = enabled; }
        bool IsEnabled() const { return mRequestedEnabled; }
        bool IsSupported() const { return mQueryPool != VK_NULL_HANDLE; }
        bool IsRecording() const { return mRecording; }
        const GPUTimings& GetResults() const { return mResults; }

    public:
        static constexpr Uint MAX_SCOPES = 255; // Per frame, + the frame scope
        static constexpr Uint MAX_DEPTH = 16;

    private:
        static constexpr Uint QUERIES_PER_SLOT = (MAX_SCOPES + 1) * 2;
        static constexpr Uint INVALID_SCOPE = ~0u;

        struct ScopeRecord
        {
            String Name;
            Uint Depth = 0;
            bool Wait = false;
        };

        struct Slot
        {
            Vector<ScopeRecord> Scopes; // Never shrinks, names keep their capacity
            Uint ScopeCount = 0;
            Uint QueryCount = 0;  // Queries written by the last submission of this slot, 0 = nothing to read
            bool Overflowed = false;
        };

        void WriteTimestamp(VkCommandBuffer cmd, Uint query) const;

    private:
        VkQueryPool mQueryPool = VK_NULL_HANDLE;
        float mNanosecondsPerTick = 1.0f;
        uint64_t mTimestampMask = ~0ull;

        std::array<Slot, RHISettings::FRAMES_IN_FLIGHT> mSlots;
        std::array<Uint, MAX_DEPTH> mOpenScopes = {}; // Scope index per depth, INVALID_SCOPE for dropped ones
        Uint mOpenCount = 0;
        Uint mExtraDepth = 0; // Scopes opened past MAX_DEPTH (dropped)
        Uint mCurrentSlot = 0;

        Vector<uint64_t> mReadback;
        GPUTimings mResults;

        bool mRequestedEnabled = false;
        bool mRecording = false; // Timing the frame being recorded
    };

} // namespace Surge
