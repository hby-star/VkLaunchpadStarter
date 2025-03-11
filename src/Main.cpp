/*
 * Copyright (c) 2023 TU Wien, Institute of Visual Computing & Human-Centered Technology.
 */

 // Include our framework and the Vulkan headers:
#include "VulkanLaunchpad.h"
#include <vulkan/vulkan.h>
#include "Camera.h"

// Include some local helper functions:
#include "VulkanHelpers.h"
#include "Teapot.h"
#include "LoadModel.hpp"

// Include functionality from the standard library:
#include <vector>
#include <unordered_map>
#include <limits>
#include <algorithm>
#include <array>
#include <string>

/* ------------------------------------------------ */
// Some more little helpers directly declared here:
// (Definitions of functions below the main function)
/* ------------------------------------------------ */

/*!
 *	This callback function gets invoked by GLFW whenever a GLFW error occured.
 */
void errorCallbackFromGlfw(int error, const char* description);

/*!
 *	Function that is invoked by GLFW to handle key events like key presses or key releases.
 *	If the ESC key has been pressed, the window will be marked that it should close.
 */
void handleGlfwKeyCallback(GLFWwindow* glfw_window, int key, int scancode, int action, int mods);

/*!
 *	Function that can be used to query whether or not currently, i.e. NOW, a certain button
 *  is pressed down, or not.
 *  @param	glfw_key_code	One of the GLFW key codes.
 *                          I.e., use one of the defines that start with GLFW_KEY_*
 *  @return True if the given key is currently pressed down, false otherwise (i.e. released).
 */
bool isKeyDown(int glfw_key_code);

/*!
 *	Determine the Vulkan instance extensions that are required by GLFW and Vulkan Launchpad.
 *	Required extensions from both sources are combined into one single vector (i.e., in
 *	contiguous memory, so that they can easily be passed to:
 *  VkInstanceCreateInfo::enabledExtensionCount and to VkInstanceCreateInfo::ppEnabledExtensionNames.
 *	@return     A std::vector of const char* elements, containing all required instance extensions.
 *	@example    std::vector<const char*> extensions = getRequiredInstanceExtensions();
 *	            VkInstanceCreateInfo create_info    = {};
 *	            create_info.enabledExtensionCount   = extensions.size();
 *	            create_info.ppEnabledExtensionNames = extensions.data();
 */
std::vector<const char*> getRequiredInstanceExtensions();

/*!
 *	Based on the given physical device and the surface, select a queue family which supports both,
 *	graphics and presentation to the given surface. Return the INDEX of an appropriate queue family!
 *	@return		The index of a queue family which supports the required features shall be returned.
 */
uint32_t selectQueueFamilyIndex(VkPhysicalDevice physical_device, VkSurfaceKHR surface);

/* ------------------------------------------------ */
// Main
/* ------------------------------------------------ */

const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

SwapChainSupportDetails querySwapChainSupport(VkSurfaceKHR surface, VkPhysicalDevice device)
{
	SwapChainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

	if (formatCount != 0)
	{
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

	if (presentModeCount != 0)
	{
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
	}

	return details;
}

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
	for (const auto& availableFormat : availableFormats)
	{
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return availableFormat;
		}
	}

	return availableFormats[0];
}

VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
	for (const auto& availablePresentMode : availablePresentModes)
	{
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return availablePresentMode;
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D chooseSwapExtent(GLFWwindow* window, const VkSurfaceCapabilitiesKHR& capabilities)
{
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		return capabilities.currentExtent;
	}
	else
	{
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		VkExtent2D actualExtent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)
		};

		actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

		return actualExtent;
	}
}

uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
	{
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			return i;
		}
	}

	throw std::runtime_error("failed to find suitable memory type!");
}

void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create buffer!");
	}

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate buffer memory!");
	}

	vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

VkFormat findSupportedFormat(VkPhysicalDevice physicalDevice, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
	for (VkFormat format : candidates)
	{
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
		{
			return format;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
		{
			return format;
		}
	}

	throw std::runtime_error("failed to find supported format!");
}

VkFormat findDepthFormat(VkPhysicalDevice physicalDevice)
{
	return findSupportedFormat(physicalDevice,
		{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);
}

bool hasStencilComponent(VkFormat format)
{
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

VkImageView createImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels)
{
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = aspectFlags;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = mipLevels;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	VkImageView imageView;
	if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create image view!");
	}

	return imageView;
}

void createImage(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory)
{
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = mipLevels;
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage;
	imageInfo.samples = numSamples;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create image!");
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(device, image, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate image memory!");
	}

	vkBindImageMemory(device, image, imageMemory, 0);
}


struct MVPMatrix
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

struct LightInfo
{
	glm::vec3 lightPos;
	glm::vec3 lightColor;
	glm::vec3 camPos;
};

void updateMVPBuffer(void* mvpBufferMapped, MVPMatrix mvp)
{
	memcpy(mvpBufferMapped, &mvp, sizeof(mvp));
}

void updateLightBuffer(void* lightBufferMapped, LightInfo light)
{
	memcpy(lightBufferMapped, &light, sizeof(light));
}

int main(int argc, char** argv)
{
	VKL_LOG(":::::: WELCOME TO VULKAN LAUNCHPAD ::::::");

	// Install a callback function, which gets invoked whenever a GLFW error occurred:
	glfwSetErrorCallback(errorCallbackFromGlfw);

	// Initialize GLFW:
	if (!glfwInit())
	{
		VKL_EXIT_WITH_ERROR("Failed to init GLFW");
	}

	/* --------------------------------------------- */
	// Task 1.1: Create a Window with GLFW
	/* --------------------------------------------- */
	constexpr int window_width = 800;
	constexpr int window_height = 800;
	constexpr bool fullscreen = false;
	constexpr char* window_title = "Tutorial Window";

	// Use a monitor if we'd like to open the window in fullscreen mode:
	GLFWmonitor* monitor = nullptr;
	if (fullscreen)
	{
		monitor = glfwGetPrimaryMonitor();
	}

	// Set some window settings before creating the window:
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // No need to create a graphics context for Vulkan
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	// Get a valid window handle and assign to window:
	GLFWwindow* window = glfwCreateWindow(window_width, window_height, window_title, monitor, nullptr);
	VklCameraHandle camera = vklCreateCamera(window);

	if (!window)
	{
		VKL_LOG("If your program reaches this point, that means two things:");
		VKL_LOG("1) Project setup was successful. Everything is working fine.");
		VKL_LOG("2) You haven't implemented the first task, which is creating a window with GLFW.");
		VKL_EXIT_WITH_ERROR("No GLFW window created.");
	}
	VKL_LOG("Task 1.1 done.");

	// Set up a key callback via GLFW here to handle keyboard user input:
	glfwSetKeyCallback(window, handleGlfwKeyCallback);

	/* --------------------------------------------- */
	// Task 1.2: Create a Vulkan Instance
	/* --------------------------------------------- */
	VkInstance vk_instance = VK_NULL_HANDLE;

	// Describe some meta data about this application, and define which Vulkan API version is required:
	VkApplicationInfo application_info = {};                     // Zero-initialize every member
	application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO; // Set this struct instance's type
	application_info.pEngineName = "Vulkan Launchpad";           // Set some properties...
	application_info.engineVersion = VK_MAKE_API_VERSION(0, 2023, 1, 0);
	application_info.pApplicationName = "An Introduction to Vulkan";
	application_info.applicationVersion = VK_MAKE_API_VERSION(0, 2023, 1, 1);
	application_info.apiVersion = VK_API_VERSION_1_1;            // Your system needs to support this Vulkan API version.

	// We'll require some extensions (e.g., for presenting something on a window surface, and more):
	std::vector<const char*> required_extensions = getRequiredInstanceExtensions();

	// Layers enable additional functionality. We'd like to enable the standard validation layer, 
	// so that we get meaningful and descriptive error messages whenever we messed up something:
	if (!hlpIsInstanceLayerSupported("VK_LAYER_KHRONOS_validation"))
	{
		VKL_EXIT_WITH_ERROR("Validation layer \"VK_LAYER_KHRONOS_validation\" is not supported.");
	}
	VKL_LOG("Validation layer \"VK_LAYER_KHRONOS_validation\" is supported.");
	std::vector<const char*> enabled_layers{ "VK_LAYER_KHRONOS_validation" };

	// Tie everything from above together in an instance of VkInstanceCreateInfo:
	VkInstanceCreateInfo instance_create_info = {}; // Zero-initialize every member
	instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO; // Set this struct instance's type
	instance_create_info.pApplicationInfo = &application_info;
	// Hook in required_extensions using VkInstanceCreateInfo::enabledExtensionCount and VkInstanceCreateInfo::ppEnabledExtensionNames!
	instance_create_info.enabledLayerCount = static_cast<uint32_t>(enabled_layers.size());
	instance_create_info.ppEnabledLayerNames = enabled_layers.data();

	// Hook in enabled_layers using VkInstanceCreateInfo::enabledLayerCount and VkInstanceCreateInfo::ppEnabledLayerNames!
	instance_create_info.enabledExtensionCount = static_cast<uint32_t>(required_extensions.size());
	instance_create_info.ppEnabledExtensionNames = required_extensions.data();

	// Use vkCreateInstance to create a vulkan instance handle! Assign it to vk_instance!
	VkResult result = vkCreateInstance(&instance_create_info, nullptr, &vk_instance);
	VKL_CHECK_VULKAN_RESULT(result);

	if (!vk_instance)
	{
		VKL_EXIT_WITH_ERROR("No VkInstance created or handle not assigned.");
	}
	VKL_LOG("Task 1.2 done.");

	/* --------------------------------------------- */
	// Task 1.3: Create a Vulkan Window Surface
	/* --------------------------------------------- */
	VkSurfaceKHR vk_surface = VK_NULL_HANDLE;

	// Use glfwCreateWindowSurface to create a window surface! Assign its handle to vk_surface!
	result = glfwCreateWindowSurface(vk_instance, window, nullptr, &vk_surface);
	VKL_CHECK_VULKAN_RESULT(result);

	if (!vk_surface)
	{
		VKL_EXIT_WITH_ERROR("No VkSurfaceKHR created or handle not assigned.");
	}
	VKL_LOG("Task 1.3 done.");

	/* --------------------------------------------- */
	// Task 1.4 Pick a Physical Device
	/* --------------------------------------------- */
	VkPhysicalDevice vk_physical_device = VK_NULL_HANDLE;

	// Use vkEnumeratePhysicalDevices get all the available physical device handles! 
	//       Select one that is suitable using hlpSelectPhysicalDeviceIndex and assign it to vk_physical_device!
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(vk_instance, &deviceCount, nullptr);
	if (deviceCount == 0)
	{
		VKL_EXIT_WITH_ERROR("No physical devices found.");
	}
	std::vector<VkPhysicalDevice> physical_devices(deviceCount);
	vkEnumeratePhysicalDevices(vk_instance, &deviceCount, physical_devices.data());
	uint32_t selected_physical_device_index = hlpSelectPhysicalDeviceIndex(physical_devices, vk_surface);
	vk_physical_device = physical_devices[selected_physical_device_index];
	// Display the selected physical device info
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(vk_physical_device, &deviceProperties);
	VKL_LOG("Selected physical device: " << deviceProperties.deviceName);

	//result = VK_ERROR_INITIALIZATION_FAILED;
	//VKL_CHECK_VULKAN_RESULT(result);

	if (!vk_physical_device)
	{
		VKL_EXIT_WITH_ERROR("No VkPhysicalDevice selected or handle not assigned.");
	}
	VKL_LOG("Task 1.4 done.");

	/* --------------------------------------------- */
	// Task 1.5: Select a Queue Family
	/* --------------------------------------------- */

	// Find a suitable queue family and assign its index to the following variable:
	//       Hint: Use selectQueueFamilyIndex, but complete its implementation before!
	uint32_t selected_queue_family_index = std::numeric_limits<uint32_t>::max();
	selected_queue_family_index = selectQueueFamilyIndex(vk_physical_device, vk_surface);

	// Sanity check if we have selected a valid queue family index:
	uint32_t queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(vk_physical_device, &queue_family_count, nullptr);
	if (selected_queue_family_index >= queue_family_count)
	{
		VKL_EXIT_WITH_ERROR("Invalid queue family index selected.");
	}
	VKL_LOG("Task 1.5 done.");

	/* --------------------------------------------- */
	// Task 1.6: Create a Logical Device and Get Queue
	/* --------------------------------------------- */
	VkDevice vk_device = VK_NULL_HANDLE;
	VkQueue  vk_queue = VK_NULL_HANDLE;

	constexpr float queue_priority = 1.0f;

	VkDeviceQueueCreateInfo queue_create_info = {};
	queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue_create_info.queueFamilyIndex = selected_queue_family_index;
	queue_create_info.queueCount = 1;
	queue_create_info.pQueuePriorities = &queue_priority;

	// Create an instance of VkDeviceCreateInfo and use it to create one queue!
	//        - Hook in queue_create_info at the right place!
	//        - Use VkDeviceCreateInfo::enabledExtensionCount and VkDeviceCreateInfo::ppEnabledExtensionNames
	//         to enable the VK_KHR_SWAPCHAIN_EXTENSION_NAME device extension!
	//        - The other parameters are not required (ensure that they are zero-initialized).
	//       Finally, use vkCreateDevice to create the device and assign its handle to vk_device!
	VkPhysicalDeviceFeatures deviceFeatures{};
	deviceFeatures.fillModeNonSolid = VK_TRUE;

	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.queueCreateInfoCount = 1;
	createInfo.pQueueCreateInfos = &queue_create_info;
	createInfo.pEnabledFeatures = &deviceFeatures;
	createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	createInfo.ppEnabledExtensionNames = deviceExtensions.data();
	createInfo.enabledLayerCount = 0;

	result = vkCreateDevice(vk_physical_device, &createInfo, nullptr, &vk_device);
	VKL_CHECK_VULKAN_RESULT(result);

	if (!vk_device)
	{
		VKL_EXIT_WITH_ERROR("No VkDevice created or handle not assigned.");
	}

	// After device creation, use vkGetDeviceQueue to get the one and only created queue!
	//       Assign its handle to vk_queue!
	vkGetDeviceQueue(vk_device, selected_queue_family_index, 0, &vk_queue);

	if (!vk_queue)
	{
		VKL_EXIT_WITH_ERROR("No VkQueue selected or handle not assigned.");
	}
	VKL_LOG("Task 1.6 done.");

	/* --------------------------------------------- */
	// Task 1.7: Create Swap Chain
	/* --------------------------------------------- */
	VkSwapchainKHR vk_swapchain = VK_NULL_HANDLE;

	VkSurfaceCapabilitiesKHR surface_capabilities = hlpGetPhysicalDeviceSurfaceCapabilities(vk_physical_device, vk_surface);

	// Build the swapchain config struct:
	VkSwapchainCreateInfoKHR swapchain_create_info = {};
	swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchain_create_info.surface = vk_surface;
	swapchain_create_info.minImageCount = surface_capabilities.minImageCount;
	swapchain_create_info.imageArrayLayers = 1u;
	swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchain_create_info.preTransform = surface_capabilities.currentTransform;
	swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchain_create_info.clipped = VK_TRUE;
	swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	// Provide values for:
	//        - VkSwapchainCreateInfoKHR::queueFamilyIndexCount
	//        - VkSwapchainCreateInfoKHR::pQueueFamilyIndices
	//        - VkSwapchainCreateInfoKHR::imageFormat
	//        - VkSwapchainCreateInfoKHR::imageColorSpace
	//        - VkSwapchainCreateInfoKHR::imageExtent
	//        - VkSwapchainCreateInfoKHR::presentMode
	uint32_t queueFamilyIndices[] = { selected_queue_family_index };
	SwapChainSupportDetails swapChainSupport = querySwapChainSupport(vk_surface, vk_physical_device);

	VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
	VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
	VkExtent2D extent = chooseSwapExtent(window, swapChainSupport.capabilities);

	swapchain_create_info.queueFamilyIndexCount = 1;
	swapchain_create_info.pQueueFamilyIndices = queueFamilyIndices;
	swapchain_create_info.imageFormat = surfaceFormat.format;
	swapchain_create_info.imageColorSpace = surfaceFormat.colorSpace;
	swapchain_create_info.imageExtent = extent;
	swapchain_create_info.presentMode = presentMode;


	// Create the swapchain using vkCreateSwapchainKHR and assign its handle to vk_swapchain!
	result = vkCreateSwapchainKHR(vk_device, &swapchain_create_info, nullptr, &vk_swapchain);
	VKL_CHECK_VULKAN_RESULT(result);

	if (!vk_swapchain)
	{
		VKL_EXIT_WITH_ERROR("No VkSwapchainKHR created or handle not assigned.");
	}

	// Create a vector of VkImages with enough memory for all the swap chain's images:
	std::vector<VkImage> swap_chain_images(surface_capabilities.minImageCount);
	// Use vkGetSwapchainImagesKHR to write VkImage handles into swap_chain_images.data()!
	result = vkGetSwapchainImagesKHR(vk_device, vk_swapchain, &surface_capabilities.minImageCount, swap_chain_images.data());
	VKL_CHECK_VULKAN_RESULT(result);

	if (swap_chain_images.empty())
	{
		VKL_EXIT_WITH_ERROR("Swap chain images not retrieved.");
	}
	VKL_LOG("Task 1.7 done.");

	/* --------------------------------------------- */
	// Task 1.8: Initialize Vulkan Launchpad
	/* --------------------------------------------- */

	VkImage depthImage;
	VkDeviceMemory depthImageMemory;
	VkImageView depthImageView;

	VkFormat depthFormat = findDepthFormat(vk_physical_device);

	createImage(vk_device, vk_physical_device, swapchain_create_info.imageExtent.width, swapchain_create_info.imageExtent.height, 1, VK_SAMPLE_COUNT_1_BIT, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory);
	depthImageView = createImageView(vk_device, depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);


	// Gather swapchain config as required by the framework:
	VklSwapchainConfig swapchain_config = {};
	swapchain_config.imageExtent = swapchain_create_info.imageExtent;
	swapchain_config.swapchainHandle = vk_swapchain;
	for (VkImage vk_image : swap_chain_images)
	{
		VklSwapchainFramebufferComposition framebufferData;
		// Fill the data for the color attachment:
		//  - VklSwapchainImageDetails::imageHandle
		//  - VklSwapchainImageDetails::imageFormat
		//  - VklSwapchainImageDetails::imageUsage
		//  - VklSwapchainImageDetails::clearValue
		framebufferData.colorAttachmentImageDetails.imageHandle = vk_image;
		framebufferData.colorAttachmentImageDetails.imageFormat = swapchain_create_info.imageFormat;
		framebufferData.colorAttachmentImageDetails.imageUsage = swapchain_create_info.imageUsage;
		framebufferData.colorAttachmentImageDetails.clearValue = { 0.0f, 0.0f, 0.0f, 1.0f }; // Clear color to black

		// We don't need the depth attachment now, but keep it in mind for later!
		framebufferData.depthAttachmentImageDetails.imageHandle = depthImage;
		framebufferData.depthAttachmentImageDetails.imageFormat = depthFormat;
		framebufferData.depthAttachmentImageDetails.imageUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		framebufferData.depthAttachmentImageDetails.clearValue = { 1.0f, 0 }; // Clear depth to 1.0

		// Add it to the vector:
		swapchain_config.swapchainImages.push_back(framebufferData);
	}

	// Init the framework:
	if (!vklInitFramework(vk_instance, vk_surface, vk_physical_device, vk_device, vk_queue, swapchain_config))
	{
		VKL_EXIT_WITH_ERROR("Failed to init Vulkan Launchpad");
	}
	VKL_LOG("Task 1.8 done.");

	/* --------------------------------------------- */
	// Task 1.9:  Implement the Render Loop
	/* --------------------------------------------- */

	// Create a descriptor set layout
	VkDescriptorSetLayout descriptorSetLayout;

	VkDescriptorSetLayoutBinding mvpLayoutBinding{};
	mvpLayoutBinding.binding = 0;
	mvpLayoutBinding.descriptorCount = 1;
	mvpLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	mvpLayoutBinding.pImmutableSamplers = nullptr;
	mvpLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutBinding lightLayoutBinding{};
	lightLayoutBinding.binding = 1;
	lightLayoutBinding.descriptorCount = 1;
	lightLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	lightLayoutBinding.pImmutableSamplers = nullptr;
	lightLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	std::vector<VkDescriptorSetLayoutBinding> layoutBindings = { mvpLayoutBinding, lightLayoutBinding };

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
	layoutInfo.pBindings = layoutBindings.data();

	if (vkCreateDescriptorSetLayout(vk_device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create descriptor set layout!");
	}

	// Create a descriptor pool
	std::array<VkDescriptorPoolSize, 2> poolSizes{};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = 1;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[1].descriptorCount = 1;

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = 1;

	VkDescriptorPool descriptorPool;

	if (vkCreateDescriptorPool(vk_device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create descriptor pool!");
	}

	// Allocate a descriptor set
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &descriptorSetLayout;

	VkDescriptorSet descriptorSet;
	if (vkAllocateDescriptorSets(vk_device, &allocInfo, &descriptorSet) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate descriptor set!");
	}

	VkBuffer mvpBuffer;
	VkDeviceMemory mvpBufferMemory;
	void* mvpBufferMapped;
	VkDeviceSize mvpBufferSize = sizeof(MVPMatrix);
	createBuffer(vk_device, vk_physical_device, mvpBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, mvpBuffer, mvpBufferMemory);
	vkMapMemory(vk_device, mvpBufferMemory, 0, mvpBufferSize, 0, &mvpBufferMapped);
	VkDescriptorBufferInfo mvpBufferInfo{};
	mvpBufferInfo.buffer = mvpBuffer;
	mvpBufferInfo.offset = 0;
	mvpBufferInfo.range = mvpBufferSize;

	VkBuffer lightBuffer;
	VkDeviceMemory lightBufferMemory;
	void* lightBufferMapped;
	VkDeviceSize lightBufferSize = sizeof(LightInfo);
	createBuffer(vk_device, vk_physical_device, lightBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, lightBuffer, lightBufferMemory);
	vkMapMemory(vk_device, lightBufferMemory, 0, lightBufferSize, 0, &lightBufferMapped);
	VkDescriptorBufferInfo lightBufferInfo{};
	lightBufferInfo.buffer = lightBuffer;
	lightBufferInfo.offset = 0;
	lightBufferInfo.range = lightBufferSize;

	std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
	descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[0].dstSet = descriptorSet;
	descriptorWrites[0].dstBinding = 0;
	descriptorWrites[0].dstArrayElement = 0;
	descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorWrites[0].descriptorCount = 1;
	descriptorWrites[0].pBufferInfo = &mvpBufferInfo;
	descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[1].dstSet = descriptorSet;
	descriptorWrites[1].dstBinding = 1;
	descriptorWrites[1].dstArrayElement = 0;
	descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorWrites[1].descriptorCount = 1;
	descriptorWrites[1].pBufferInfo = &lightBufferInfo;

	vkUpdateDescriptorSets(vk_device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

	// Create a graphics pipeline
	VkVertexInputBindingDescription bindingDescription = Vertex::getBindingDescription();
	std::vector<VkVertexInputAttributeDescription> inputAttributeDescriptions = Vertex::getAttributeDescriptions();

	VklGraphicsPipelineConfig pipeline_config = {};
	pipeline_config.vertexShaderPath = "C:\\Users\\Admin\\Desktop\\Learn\\Vulkan\\VkLaunchpadStarter\\assets\\shader\\shader.vert";
	pipeline_config.fragmentShaderPath = "C:\\Users\\Admin\\Desktop\\Learn\\Vulkan\\VkLaunchpadStarter\\assets\\shader\\shader.frag";
	pipeline_config.descriptorLayout = { mvpLayoutBinding, lightLayoutBinding };
	pipeline_config.vertexInputBuffers = { bindingDescription };
	pipeline_config.inputAttributeDescriptions = inputAttributeDescriptions;
	pipeline_config.polygonDrawMode = VK_POLYGON_MODE_FILL;
	pipeline_config.triangleCullingMode = VK_CULL_MODE_BACK_BIT;
	VkPipeline customPipeline = vklCreateGraphicsPipeline(pipeline_config);

	const std::string modelPathCube = "C:\\Users\\Admin\\Desktop\\Learn\\Vulkan\\VkLaunchpadStarter\\assets\\cube\\cube.obj";
	const std::string modelPathSphere = "C:\\Users\\Admin\\Desktop\\Learn\\Vulkan\\VkLaunchpadStarter\\assets\\sphere\\sphere.obj";
	Model model;
	model.createGeometryAndBuffers(modelPathCube, glm::vec3(-1.5f, 0.0f, 0.0f));
	model.createGeometryAndBuffers(modelPathSphere, glm::vec3(1.5f, 0.0f, 0.0f));

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents(); // Handle user input

		vklWaitForNextSwapchainImage();
		vklStartRecordingCommands();

		vklUpdateCamera(camera);
		MVPMatrix mvp{};
		mvp.model = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, 1.0f, 1.0f));
		mvp.view = vklGetCameraViewMatrix(camera);
		mvp.proj = vklGetCameraProjectionMatrix(camera);
		LightInfo light{};
		light.lightPos = glm::vec3(5.0f, 5.0f, 5.0f);
		light.lightColor = glm::vec3(1.0f, 1.0f, 1.0f);
		light.camPos = vklGetCameraPosition(camera);
		updateMVPBuffer(mvpBufferMapped, mvp);
		updateLightBuffer(lightBufferMapped, light);

		model.drawAllModel(customPipeline, descriptorSet);

		vklEndRecordingCommands();
		vklPresentCurrentSwapchainImage();
	}

	VKL_LOG("Task 1.9 done.");

	// Wait for all GPU work to finish before cleaning up:
	vkDeviceWaitIdle(vk_device);

	/* --------------------------------------------- */
	// Task 1.10: Cleanup
	/* --------------------------------------------- */

	// Destroy the graphics pipeline
	vklDestroyGraphicsPipeline(customPipeline);
	// Destory the buffer
	vkDestroyBuffer(vk_device, mvpBuffer, nullptr);
	vkFreeMemory(vk_device, mvpBufferMemory, nullptr);
	vkDestroyBuffer(vk_device, lightBuffer, nullptr);
	vkFreeMemory(vk_device, lightBufferMemory, nullptr);
	// Destroy the descriptor set layout
	vkDestroyDescriptorSetLayout(vk_device, descriptorSetLayout, nullptr);
	// Destroy the descriptor pool
	vkDestroyDescriptorPool(vk_device, descriptorPool, nullptr);

	// Every vkCreate should have a vkDestroy£¬follow this rule to clean up resources.
	// Destroy the teapot buffers in 1.9
	//teapotDestroyBuffers();
	model.destroyAllModelBuffers();
	// Destroy the framework in 1.8
	vklDestroyFramework();
	vkDestroyImage(vk_device, depthImage, nullptr);
	vkFreeMemory(vk_device, depthImageMemory, nullptr);
	vkDestroyImageView(vk_device, depthImageView, nullptr);
	// Destroy the swapchain in 1.7
	vkDestroySwapchainKHR(vk_device, vk_swapchain, nullptr);
	// Destroy the logical device in 1.6
	vkDestroyDevice(vk_device, nullptr);
	// Destroy the surface in 1.3
	vkDestroySurfaceKHR(vk_instance, vk_surface, nullptr);
	// Destroy the instance in 1.2
	vkDestroyInstance(vk_instance, nullptr);
	// Destroy the window in 1.1
	glfwDestroyWindow(window);
	glfwTerminate();

	VKL_LOG("Task 1.10 done.");
	VKL_LOG("Bye~");


	return EXIT_SUCCESS;
}

/* ------------------------------------------------ */
// Definitions of little helpers defined above main:
/* ------------------------------------------------ */

void errorCallbackFromGlfw(int error, const char* description)
{
	std::cout << "GLFW error " << error << ": " << description << std::endl;
}

std::unordered_map<int, bool> g_isGlfwKeyDown;

void handleGlfwKeyCallback(GLFWwindow* glfw_window, int key, int scancode, int action, int mods)
{
	if (action == GLFW_PRESS)
	{
		g_isGlfwKeyDown[key] = true;
	}

	if (action == GLFW_RELEASE)
	{
		g_isGlfwKeyDown[key] = false;
	}

	// We mark the window that it should close if ESC is pressed:
	if (action == GLFW_RELEASE && key == GLFW_KEY_ESCAPE)
	{
		glfwSetWindowShouldClose(glfw_window, true);
	}
}

bool isKeyDown(int glfw_key_code)
{
	return g_isGlfwKeyDown[glfw_key_code];
}

std::vector<const char*> getRequiredInstanceExtensions()
{
	// Get extensions which GLFW requires:
	uint32_t num_glfw_extensions;
	const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&num_glfw_extensions);

	// Get extensions which Vulkan Launchpad requires:
	uint32_t num_vkl_extensions;
	const char** vkl_extensions = vklGetRequiredInstanceExtensions(&num_vkl_extensions);

	// Merge both arrays of extensions:
	std::vector<const char*> all_required_extensions(glfw_extensions, glfw_extensions + num_glfw_extensions);
	all_required_extensions.insert(all_required_extensions.end(), vkl_extensions, vkl_extensions + num_vkl_extensions);

	// Perform a sanity check if all the extensions are really supported by Vulkan on 
	// this system (if they are not, we have a problem):
	for (auto ext : all_required_extensions)
	{
		if (!hlpIsInstanceExtensionSupported(ext))
		{
			VKL_EXIT_WITH_ERROR("Required extension \"" << ext << "\" is not supported");
		}
		VKL_LOG("Extension \"" << ext << "\" is supported");
	}

	return all_required_extensions;
}

uint32_t selectQueueFamilyIndex(VkPhysicalDevice physical_device, VkSurfaceKHR surface)
{
	// Get the number of different queue families for the given physical device:
	uint32_t queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);

	// Get the queue families' data:
	std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families.data());

	// Find a suitable queue family index and return it!
	uint32_t index = 0;
	for (const auto& queueFamily : queue_families)
	{
		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, index, surface, &presentSupport);
			if (presentSupport)
			{
				VKL_LOG("Find queue family index " << index << " supports both graphics and presentation.");
				return index;
			}
		}

		index++;
	}

	VKL_EXIT_WITH_ERROR("Unable to find a suitable queue family that supports graphics and presentation on the same queue.");
}
