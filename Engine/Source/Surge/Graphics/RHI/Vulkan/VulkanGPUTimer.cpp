// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/RHI/Vulkan/VulkanGPUTimer.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanRHI.hpp"

namespace Surge
{
    void VulkanGPUTimer::Initialize(const VulkanRHI& rhi, bool enabled)
    {
        mRequestedEnabled = enabled;

        Uint familyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(rhi.GetGPU(), &familyCount, nullptr);
        Vector<VkQueueFamilyProperties> families(familyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(rhi.GetGPU(), &familyCount, families.data());

        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(rhi.GetGPU(), &props);

        const Uint validBits = families[static_cast<Uint>(rhi.GetQueueIndex())].timestampValidBits;
        if(validBits == 0 || props.limits.timestampPeriod <= 0.0f)
        {
            Log<Severity::Warn>("[VulkanGPUTimer::Initialize] The graphics queue in this GPU has no timestamp support, GPUTiming is unavailable");
            return;
        }

        mTimestampMask = validBits >= 64 ? ~0ull : (1ull << validBits) - 1;
        mNanosecondsPerTick = props.limits.timestampPeriod;

        VkQueryPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        poolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        poolInfo.queryCount = QUERIES_PER_SLOT * RHISettings::FRAMES_IN_FLIGHT;
        VK_CALL(vkCreateQueryPool(rhi.GetDevice(), &poolInfo, nullptr, &mQueryPool));

        for(Slot& slot : mSlots)
            slot.Scopes.resize(MAX_SCOPES);
        mReadback.resize(QUERIES_PER_SLOT);
        mResults.Scopes.reserve(MAX_SCOPES);
    }

    void VulkanGPUTimer::Shutdown(const VulkanRHI& rhi)
    {
        if(mQueryPool != VK_NULL_HANDLE)
        {
            vkDestroyQueryPool(rhi.GetDevice(), mQueryPool, nullptr);
            mQueryPool = VK_NULL_HANDLE;
        }
    }

    void VulkanGPUTimer::Collect(const VulkanRHI& rhi, Uint frameIndex)
    {
        Slot& slot = mSlots[frameIndex];
        if(slot.QueryCount == 0)
        {
            if(!mRequestedEnabled && mResults.Valid)
            {
                mResults.Scopes.clear();
                mResults.Valid = false;
            }
            return;
        }

        // The slot's fence has signaled: every query is available, no WAIT flag needed
        const Uint queryCount = slot.QueryCount;
        slot.QueryCount = 0;
        const VkResult result = vkGetQueryPoolResults(rhi.GetDevice(), mQueryPool, frameIndex * QUERIES_PER_SLOT, queryCount, queryCount * sizeof(uint64_t), mReadback.data(), sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
        if(result != VK_SUCCESS)
            return;

        const auto toMilliseconds = [this](uint64_t begin, uint64_t end) {
            return static_cast<float>(static_cast<double>((end - begin) & mTimestampMask) * mNanosecondsPerTick * 1e-6);
        };

        mResults.FrameMilliseconds = toMilliseconds(mReadback[0], mReadback[1]);

        // Wait scopes are not work: left out of Scopes, their time taken off the enclosing scopes (pre-order, so a scope's ancestors are
        // the last scopes seen at each smaller depth). Sized once, the result strings keep their capacity across frames
        Uint workCount = 0;
        for(Uint i = 0; i < slot.ScopeCount; i++)
            workCount += slot.Scopes[i].Wait ? 0 : 1;
        mResults.Scopes.resize(workCount);

        std::array<Uint, MAX_DEPTH> ancestors = {};
        Uint written = 0;
        mResults.DisplayWaitMilliseconds = 0.0f;
        for(Uint i = 0; i < slot.ScopeCount; i++)
        {
            const ScopeRecord& record = slot.Scopes[i];
            const float milliseconds = toMilliseconds(mReadback[2 + i * 2], mReadback[3 + i * 2]);
            if(record.Wait)
            {
                mResults.DisplayWaitMilliseconds += milliseconds;
                for(Uint depth = 0; depth < record.Depth; depth++)
                    mResults.Scopes[ancestors[depth]].Milliseconds -= milliseconds;
                continue;
            }

            GPUTimingScope& scope = mResults.Scopes[written];
            scope.Name = record.Name;
            scope.Depth = record.Depth;
            scope.Milliseconds = milliseconds;
            ancestors[record.Depth] = written++;
        }
        mResults.Overflowed = slot.Overflowed;
        mResults.Valid = true;
        mResults.ResolvedFrames++;
    }

    void VulkanGPUTimer::BeginFrame(VkCommandBuffer cmd, Uint frameIndex)
    {
        mRecording = mRequestedEnabled && mQueryPool != VK_NULL_HANDLE;
        mCurrentSlot = frameIndex;
        mOpenCount = 0;
        mExtraDepth = 0;

        Slot& slot = mSlots[frameIndex];
        slot.ScopeCount = 0;
        slot.Overflowed = false;
        if(!mRecording)
            return;

        // Queries must be reset before every use, outside of a render pass
        vkCmdResetQueryPool(cmd, mQueryPool, frameIndex * QUERIES_PER_SLOT, QUERIES_PER_SLOT);
        WriteTimestamp(cmd, 0);
    }

    void VulkanGPUTimer::EndFrame(VkCommandBuffer cmd)
    {
        if(!mRecording)
            return;

        SG_ASSERT(mOpenCount == 0 && mExtraDepth == 0, "[VulkanGPUTimer] A GPU scope was left open (CmdBeginGPUScope without CmdEndGPUScope)");
        mExtraDepth = 0;
        while(mOpenCount > 0)
            EndScope(cmd);

        WriteTimestamp(cmd, 1);

        Slot& slot = mSlots[mCurrentSlot];
        slot.QueryCount = 2 + slot.ScopeCount * 2;
        mRecording = false;
    }

    void VulkanGPUTimer::BeginScope(VkCommandBuffer cmd, std::string_view name, bool wait)
    {
        if(!mRecording)
            return;

        Slot& slot = mSlots[mCurrentSlot];
        if(mOpenCount == MAX_DEPTH)
        {
            mExtraDepth++;
            slot.Overflowed = true;
            return;
        }

        Uint index = INVALID_SCOPE;
        if(slot.ScopeCount < MAX_SCOPES)
        {
            index = slot.ScopeCount++;
            ScopeRecord& record = slot.Scopes[index];
            record.Name.assign(name); // Reuses the record's capacity
            record.Depth = mOpenCount;
            record.Wait = wait;
            WriteTimestamp(cmd, 2 + index * 2);
        }
        else
            slot.Overflowed = true;

        mOpenScopes[mOpenCount++] = index;
    }

    void VulkanGPUTimer::EndScope(VkCommandBuffer cmd)
    {
        if(!mRecording)
            return;

        if(mExtraDepth > 0)
        {
            mExtraDepth--;
            return;
        }

        if(mOpenCount == 0)
        {
            SG_ASSERT_INTERNAL("[VulkanGPUTimer] CmdEndGPUScope without a matching CmdBeginGPUScope");
            return;
        }

        const Uint index = mOpenScopes[--mOpenCount];
        if(index != INVALID_SCOPE)
            WriteTimestamp(cmd, 3 + index * 2);
    }

    void VulkanGPUTimer::WriteTimestamp(VkCommandBuffer cmd, Uint query) const
    {
        // BOTTOM_OF_PIPE: written once all previous work has finished, so consecutive scopes partition the frame
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, mQueryPool, mCurrentSlot * QUERIES_PER_SLOT + query);
    }

} // namespace Surge
