#pragma once

#include "InputManager.h"

#include <optional>
#include <vector>

class VulkanInstance
{
public:
	// Vulkan instance properties that should be shared with renderers
	struct Properties {
		VkFormat swapChainImageFormat = VK_FORMAT_UNDEFINED;
		VkExtent2D swapChainExtent = { 0 };
		VkSampleCountFlagBits maxNbMsaaSamples = VK_SAMPLE_COUNT_1_BIT;
		float maxSamplerAnisotropy = 0.f;
	};

	struct QueueFamilyIndices {
		std::optional<unsigned int> graphicsFamily;
		std::optional<unsigned int> presentationFamily;
		bool hasMandatoryFamilies();
	};

	struct SwapChainSupportDetails {
		VkSurfaceCapabilitiesKHR capabilities = { 0 };
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};

public:
	int init(GLFWwindow* window);
	void recreateSwapChain();
	int cleanup();
	void waitForIdleDevice() const;

	// Utility
	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels) const;
	VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates,
		VkImageTiling tiling, VkFormatFeatureFlags features) const;
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

	// Accessorss
	const Properties& getProperties() const;
	VkDevice& getLogicalDevice();
	const QueueFamilyIndices& getQueueFamilyIndices() const;
	const std::vector<VkImageView>& getSwapChainImageViews() const;
	VkQueue& getGraphicsQueue();
	VkQueue& getPresentationQueue();
	VkSwapchainKHR& getSwapChain();
	size_t getSwapChainSize() const;
	void cleanupSwapChain();

private:
	void _createInstance();
	void _setupDebugMessenger();
	void _createSurface();

	// Device
	void _pickPhysicalDevice();
	int _ratePhysicalDevice(VkPhysicalDevice device, QueueFamilyIndices& indices, SwapChainSupportDetails& swapChainSupportDetails);
	QueueFamilyIndices _findRequiredQueueFamilies(VkPhysicalDevice device);
	bool _areDeviceRequiredExtensionsSupported(VkPhysicalDevice device);
	SwapChainSupportDetails _querySwapChainSupport(VkPhysicalDevice device);
	void _createLogicalDevice();

	// Swap chain
	void _createSwapChain();
	VkSurfaceFormatKHR _chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR _chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D _chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
	void _createSwapChainImageViews();
	VkSampleCountFlagBits _getMaxUsableSampleCount() const;

private:
	GLFWwindow* _window = nullptr;
	VkInstance _instance = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT _debugMessenger = VK_NULL_HANDLE;
	VkSurfaceKHR _surface = VK_NULL_HANDLE;

	// Device
	VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;
	QueueFamilyIndices _queueFamilyIndices;
	SwapChainSupportDetails _swapChainSupportDetails;
	VkDevice _device = VK_NULL_HANDLE;
	VkQueue _graphicsQueue = VK_NULL_HANDLE;
	VkQueue _presentationQueue = VK_NULL_HANDLE;

	// Swap chain
	VkSwapchainKHR _swapChain = VK_NULL_HANDLE;
	std::vector<VkImage> _swapChainImages;
	std::vector<VkImageView> _swapChainImageViews;

	Properties _properties;
};
