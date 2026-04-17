// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RHI/RHIDevice.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanResources.hpp"

// Forward-declare VMA allocator type (full header only in .cpp)
typedef struct VmaAllocator_T* VmaAllocator;

// ---------------------------------------------------------------------------
// VulkanRHIDevice.hpp
//   Standalone Vulkan backend device.
//   Owns VkInstance, VkPhysicalDevice, VkDevice, and a VmaAllocator.
//   Creates and owns the RHIDevice dispatch table that is installed via
//   SetRHIDevice() to make it the active global backend.
//
//   This class is SEPARATE from the legacy Surge::VulkanDevice and does NOT
//   share its lifetime.  The old layer remains in Abstraction/Vulkan/ until
//   every call-site has been migrated to the new RHI.
// ---------------------------------------------------------------------------

namespace Surge::RHI
{
    class SURGE_API VulkanRHIDevice
    {
    public:
        VulkanRHIDevice()  = default;
        ~VulkanRHIDevice() = default;

        SURGE_DISABLE_COPY_AND_MOVE(VulkanRHIDevice);

        // Initialize the Vulkan instance, pick GPU, create logical device,
        // create VMA allocator, fill the dispatch table, and register it as
        // the global RHI device via SetRHIDevice().
        void Initialize(void* windowHandle, uint32_t width, uint32_t height);
        void Shutdown();

        // Returns a pointer to the filled RHIDevice dispatch table.
        RHIDevice* GetDispatchTable() { return &mDispatchTable; }

        // Raw accessors used by the Vulkan backend .cpp files.
        VkInstance       GetInstance()       const { return mInstance; }
        VkPhysicalDevice GetPhysicalDevice() const { return mPhysicalDevice; }
        VkDevice         GetLogicalDevice()  const { return mDevice; }
        VmaAllocator     GetVMAAllocator()         { return mVMAAllocator; }
        VkQueue          GetGraphicsQueue()  const { return mGraphicsQueue; }
        VkQueue          GetComputeQueue()   const { return mComputeQueue; }
        VkQueue          GetTransferQueue()  const { return mTransferQueue; }
        uint32_t         GetGraphicsFamily() const { return mGraphicsFamily; }
        uint32_t         GetComputeFamily()  const { return mComputeFamily; }
        uint32_t         GetTransferFamily() const { return mTransferFamily; }

    private:
        // Initialisation sub-steps
        void CreateInstance();
        void SelectPhysicalDevice();
        void CreateLogicalDevice();
        void CreateVMAAllocator();
        void FillDispatchTable();

        VkInstance       mInstance       = VK_NULL_HANDLE;
        VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
        VkDevice         mDevice         = VK_NULL_HANDLE;
        VmaAllocator     mVMAAllocator   = nullptr;

        VkQueue  mGraphicsQueue = VK_NULL_HANDLE;
        VkQueue  mComputeQueue  = VK_NULL_HANDLE;
        VkQueue  mTransferQueue = VK_NULL_HANDLE;

        uint32_t mGraphicsFamily = 0;
        uint32_t mComputeFamily  = 0;
        uint32_t mTransferFamily = 0;

        // Stored from Initialize() for use when the caller later creates the swapchain
        void*    mInitialWindowHandle = nullptr;
        uint32_t mInitialWidth        = 0;
        uint32_t mInitialHeight       = 0;

        RHIDevice mDispatchTable = {};
    };

    // The single global Vulkan backend device.
    // Initialized by the engine at startup when the new RHI path is selected.
    extern VulkanRHIDevice gVulkanRHIDevice;

} // namespace Surge::RHI
