// Copyright (c) - SurgeTechnologies - All rights reserved
#include "VulkanDebugger.hpp"
#include "VulkanRHI.hpp"
#include "Surge/Core/Logger/Logger.hpp"

static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void* userData)
{
	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
	{
		Surge::Log<Surge::Severity::Error>("{0} VulkanDebugger ERROR: {1}: {2}", callbackData->messageIdNumber, callbackData->pMessageIdName, callbackData->pMessage);
	}
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
	{
		Surge::Log<Surge::Severity::Warn>("{0} VulkanDebugger WARNING: {1}: {2}", callbackData->messageIdNumber, callbackData->pMessageIdName, callbackData->pMessage);
	}
	else if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
	{
		Surge::Log<Surge::Severity::Warn>("{0} VulkanDebugger PERFORMANCE WARNING: {1}: {2}", callbackData->messageIdNumber, callbackData->pMessageIdName, callbackData->pMessage);
	}
	else
	{
		Surge::Log<Surge::Severity::Info>("{0} VulkanDebugger INFORMATION: {1}: {2}", callbackData->messageIdNumber, callbackData->pMessageIdName, callbackData->pMessage);
	}
	return VK_FALSE;
}

namespace Surge
{
	void VulkanDebugger::Create(VkInstanceCreateInfo& vkInstanceCreateInfo)
	{		
		mDebugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		mDebugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		mDebugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		mDebugCreateInfo.pfnUserCallback = VulkanDebugCallback;
		mDebugCreateInfo.pUserData = nullptr;

		vkInstanceCreateInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&mDebugCreateInfo;
	}

	void VulkanDebugger::AddValidationLayers(Vector<const char*>& outInstanceLayers)
	{
		const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
		uint32_t instanceLayerCount;
		VK_CALL(vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr));
		Vector<VkLayerProperties> instanceLayerProperties(instanceLayerCount);
		VK_CALL(vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayerProperties.data()));

		bool validationLayerPresent = false;
		for (const VkLayerProperties& layer : instanceLayerProperties)
		{
			if (strcmp(layer.layerName, validationLayerName) == 0)
			{
				validationLayerPresent = true;
				outInstanceLayers.push_back(validationLayerName);
				break;
			}
		}

		if (!validationLayerPresent)
			Log<Severity::Error>("Validation layer requested, but it is not present ({0}), validation is disabled", validationLayerName);
		else
			Log<Severity::Info>("Vulkan Validation layer enabled");
	}

	static VkResult CreateDebugUtilsMessenger(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
	{
		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		if (func != nullptr)
		{
			Log<Severity::Info>("Creating VULKAN Debug Utils Messenger");
			return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
		}
		
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}

	static void DestroyDebugUtilsMessenger(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
	{
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func != nullptr)
			func(instance, debugMessenger, pAllocator);
	}

	void VulkanDebugger::SetDebugName(const VulkanRHI& rhi, const VkDebugUtilsObjectNameInfoEXT& nameInfo) const
	{
		vkSetDebugUtilsObjectNameEXT(rhi.GetDevice(), &nameInfo);
	}

	void VulkanDebugger::StartDiagnostics(VkInstance& instance)
	{
		VK_CALL(CreateDebugUtilsMessenger(instance, &mDebugCreateInfo, nullptr, &mDebugMessenger));
	}

	void VulkanDebugger::EndDiagnostics(VkInstance& instance)
	{
		DestroyDebugUtilsMessenger(instance, mDebugMessenger, nullptr);
	}
}
