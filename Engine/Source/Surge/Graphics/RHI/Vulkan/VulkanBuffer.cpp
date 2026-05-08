// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/RHI/Vulkan/VulkanBuffer.hpp"
#include "Surge/Graphics/RHI/Vulkan/VulkanRHI.hpp"

namespace Surge
{
	static VkBufferUsageFlags ToVkBufferUsage(BufferUsage usage)
	{
		VkBufferUsageFlags flags = 0;
		if ((Uint)usage & (Uint)BufferUsage::VERTEX) flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		if ((Uint)usage & (Uint)BufferUsage::INDEX) flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		if ((Uint)usage & (Uint)BufferUsage::UNIFORM) flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		if ((Uint)usage & (Uint)BufferUsage::STORAGE) flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		if ((Uint)usage & (Uint)BufferUsage::STAGING) flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		return flags;
	}

	BufferEntry VulkanBuffer::Create(const VulkanRHI& rhi, const BufferDesc& desc)
	{
		SG_ASSERT(desc.Size > 0, "BufferDesc::Size must be > 0");

		BufferEntry entry = {};
		entry.Desc = desc;

		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = desc.Size;
		bufferInfo.usage = ToVkBufferUsage(desc.Usage);

		// Staging buffers also need to be transfer sources
		if ((Uint)desc.Usage & (Uint)BufferUsage::STAGING)
			bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

		VmaAllocationCreateInfo allocInfo = {};
		allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

		if (desc.HostVisible)
		{
			// Persistent mapping: valid on mobile UMA, free to keep mapped forever
			allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
			allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		}

		VmaAllocator allocator = rhi.GetAllocator();
		VmaAllocationInfo vmaAllocInfo = {};
	 	VK_CALL(vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &entry.Buffer, &entry.Allocation, &vmaAllocInfo));

		// If host visible, store the persistent mapped pointer directly
		if (desc.HostVisible)
		{
			SG_ASSERT(vmaAllocInfo.pMappedData != nullptr, "VulkanBuffer: Host visible buffer failed to map");
			entry.MappedPtr = vmaAllocInfo.pMappedData;
		}

		if (desc.InitialData != nullptr)
		{
			SG_ASSERT(desc.HostVisible, "VulkanBuffer: InitialData provided but buffer is not HostVisible. Either set HostVisible = true, or upload manually via a staging buffer.");
			SG_ASSERT(entry.MappedPtr != nullptr, "VulkanBuffer: InitialData provided but MappedPtr is null.");
			memcpy(entry.MappedPtr, desc.InitialData, desc.Size);
		}


//TODO
#if defined(SURGE_DEBUG)
		if (desc.DebugName)
		{
			VkDebugUtilsObjectNameInfoEXT nameInfo = {};
			nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
			nameInfo.objectType = VK_OBJECT_TYPE_BUFFER;
			nameInfo.objectHandle = (uint64_t)entry.Buffer;
			nameInfo.pObjectName = desc.DebugName; 
			rhi.SetDebugName(nameInfo);
		}
#endif
		return entry;
	}

	void VulkanBuffer::Destroy(const VulkanRHI& rhi, BufferEntry& entry)
	{
		if (entry.Buffer != VK_NULL_HANDLE)
		{
			vmaDestroyBuffer(rhi.GetAllocator(), entry.Buffer, entry.Allocation);
			entry.Buffer = VK_NULL_HANDLE;
			entry.Allocation = VK_NULL_HANDLE;
			entry.MappedPtr = nullptr;
			entry.Desc = {};
		}
	}

	void VulkanBuffer::Upload(const VulkanRHI& rhi, BufferEntry& entry, const void* data, Uint size, Uint offset /*= 0*/)
	{
		SG_ASSERT(entry.Buffer != VK_NULL_HANDLE, "Uploading to a null buffer");
		SG_ASSERT(offset + size <= entry.Desc.Size, "Upload out of buffer bounds");
		SG_ASSERT(entry.MappedPtr != nullptr, "Upload called on a non-host-visible buffer. Use a staging buffer for GPU-only resources.");

		memcpy((uint8_t*)entry.MappedPtr + offset, data, size);
		// No explicit flush needed, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		// is required at allocation time for host visible buffers
	}
}
