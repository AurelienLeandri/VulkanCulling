#pragma once

#include "InputManager.h"

#include <optional>
#include <vector>

struct GLFWwindow;

struct AllocatedImage {
	VkImage image = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	VkImageView view = VK_NULL_HANDLE;
	uint32_t mipLevels = 1;
};

struct AllocatedBuffer {
	VkBuffer buffer = VK_NULL_HANDLE;
	VkDeviceMemory deviceMemory = VK_NULL_HANDLE;
};

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
	VulkanInstance(GLFWwindow* application);
	VulkanInstance(const VulkanInstance& other) = delete;
	VulkanInstance(const VulkanInstance&& other) = delete;
	VulkanInstance& operator=(const VulkanInstance& other) = delete;
	VulkanInstance& operator=(const VulkanInstance&& other) = delete;

public:
	void init();
	void recreateSwapChain();
	void cleanup();

	// Utility
	void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format,
		VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, AllocatedImage& image);
	void createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels, VkImageView& imageView, uint32_t baseMipLevel = 0) const;
	void createBuffer(VkCommandPool cmdPool, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, AllocatedBuffer& buffer);
	void copyBuffer(VkCommandPool cmdPool, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
	void createGPUBuffer(VkCommandPool cmdPool, VkDeviceSize size, VkBufferUsageFlags usage, const void* data, AllocatedBuffer& buffer);
	void copyBufferToImage(VkCommandPool cmdPool, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT);
	void generateMipmaps(VkCommandPool cmdPool, AllocatedImage& imageData, VkFormat imageFormat, int32_t texWidth, int32_t texHeight);
	VkCommandBuffer beginSingleTimeCommands(VkCommandPool& commandPool);
	void endSingleTimeCommands(VkCommandBuffer commandBuffer, VkCommandPool& commandPool);
	VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates,
		VkImageTiling tiling, VkFormatFeatureFlags features) const;
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
	size_t padUniformBufferSize(size_t originalSize);

	// Accessors
	const Properties& getProperties() const;
	VkDevice& getLogicalDevice();
	const QueueFamilyIndices& getQueueFamilyIndices() const;
	const std::vector<VkImageView>& getSwapChainImageViews() const;
	const std::vector<VkImage>& getSwapChainImages() const;
	std::vector<VkImage>& getSwapChainImages();
	VkQueue& getGraphicsQueue();
	VkQueue& getPresentationQueue();
	VkSwapchainKHR& getSwapChain();
	VkPhysicalDevice& getPhysicalDevice();
	VkInstance& getInstance();
	size_t getSwapChainSize() const;
	void cleanupSwapChain();

private:
	// Device
	int _ratePhysicalDevice(VkPhysicalDevice device, QueueFamilyIndices& indices, SwapChainSupportDetails& swapChainSupportDetails);
	QueueFamilyIndices _findRequiredQueueFamilies(VkPhysicalDevice device);
	bool _areDeviceRequiredExtensionsSupported(VkPhysicalDevice device);
	SwapChainSupportDetails _querySwapChainSupportDetails(VkPhysicalDevice device);

	// Swap chain
	void _createSwapChain();
	VkSurfaceFormatKHR _chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR _chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D _chooseSwapChainExtent(const VkSurfaceCapabilitiesKHR& capabilities);
	VkSampleCountFlagBits _getMaxUsableSampleCount() const;

private:
	GLFWwindow* _window = nullptr;
	VkInstance _vulkan = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT _debugMessenger = VK_NULL_HANDLE;
	VkSurfaceKHR _surface = VK_NULL_HANDLE;

	// Device
	VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;
	VkPhysicalDeviceProperties _physicalDeviceProperties = {};
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
