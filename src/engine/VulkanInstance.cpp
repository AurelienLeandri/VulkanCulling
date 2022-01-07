#include "VulkanInstance.h"

#include "DebugUtils.h"
#include "VulkanUtils.h"

#include <exception>
#include <iostream>
#include <vector>
#include <set>
#include <algorithm>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#include <glfw/glfw3.h>

namespace {
#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

    void getRequiredInstanceExtensionsNames(std::vector<const char*>& requiredExtensions);
    bool checkRequiredInstanceExtensionsSupport(const std::vector<const char*>& requiredExtensions);
    bool checkValidationLayerSupport(const std::vector<const char*>& requiredLayers);
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

    VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);

    const std::vector<const char*> debugInstanceExtensions = {
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME
    };

    const std::vector<const char*> additionalLayersDebug = {
    "VK_LAYER_KHRONOS_validation",
    "VK_LAYER_LUNARG_monitor"
    };

    const std::vector<const char*> additionalLayers = {
    "VK_LAYER_LUNARG_monitor"
    };

    const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME
    };
}

void VulkanInstance::init(GLFWwindow* window)
{
    _window = window;

    /*
    * Intance
    */

    VkInstanceCreateInfo instanceCreateInfo = {};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "LeoEngine";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    instanceCreateInfo.pApplicationInfo = &appInfo;

    // Get the extension from GLFW so that Vulkan can interface with the window system
    std::vector<const char*> requiredInstanceExtensions;
    getRequiredInstanceExtensionsNames(requiredInstanceExtensions);
    checkRequiredInstanceExtensionsSupport(requiredInstanceExtensions);
    instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(requiredInstanceExtensions.size());
    instanceCreateInfo.ppEnabledExtensionNames = requiredInstanceExtensions.data();

    // Validation layers
    if (enableValidationLayers) {
        checkValidationLayerSupport(additionalLayersDebug);
        instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(additionalLayersDebug.size());
        instanceCreateInfo.ppEnabledLayerNames = additionalLayersDebug.data();

        // Create a debug messenger specificaly for instance creation and destruction.
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        populateDebugMessengerCreateInfo(debugCreateInfo);
        instanceCreateInfo.pNext = static_cast<VkDebugUtilsMessengerCreateInfoEXT*>(&debugCreateInfo);
    }
    else {
        checkValidationLayerSupport(additionalLayers);
        instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(additionalLayers.size());
        instanceCreateInfo.ppEnabledLayerNames = additionalLayers.data();
    }

    VK_CHECK(vkCreateInstance(&instanceCreateInfo, nullptr, &_vulkan));

    /*
    * Debug messenger
    */

    if (enableValidationLayers) {
        VkDebugUtilsMessengerCreateInfoEXT debugMsgCreateInfo = {};
        populateDebugMessengerCreateInfo(debugMsgCreateInfo);

        if (auto vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(_vulkan, "vkCreateDebugUtilsMessengerEXT")) {
            VK_CHECK(vkCreateDebugUtilsMessengerEXT(_vulkan, &debugMsgCreateInfo, nullptr, &_debugMessenger));
        }
        else {
            throw VulkanRendererException("Failed to load extension function vkCreateDebugUtilsMessengerEXT.");
        }
    }

    /*
    * Surface
    */

    if (glfwCreateWindowSurface(_vulkan, _window, nullptr, &_surface)) {
        throw VulkanRendererException("Failed to create window surface.");
    }

    /*
    * Picking physical device
    */

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(_vulkan, &deviceCount, nullptr);
    if (!deviceCount) {
        throw VulkanRendererException("Failed to find GPUs with Vulkan support.");
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(_vulkan, &deviceCount, devices.data());

    const VkPhysicalDevice* candidateDevice = nullptr;
    int maxScore = 0;
    QueueFamilyIndices candidateIndices;
    SwapChainSupportDetails candidateSwapChainSupportDetails;
    for (const VkPhysicalDevice& device : devices) {
        QueueFamilyIndices indices;
        SwapChainSupportDetails swapChainSupportDetails;
        int score = _ratePhysicalDevice(device, indices, swapChainSupportDetails);
        if (score && score > maxScore) {
            candidateDevice = &device;
            candidateIndices = indices;
            candidateSwapChainSupportDetails = swapChainSupportDetails;
            maxScore = score;
        }
    }

    if (!candidateDevice) {
        throw VulkanRendererException("Failed to find a suitable device.");
    }

    _physicalDevice = *candidateDevice;
    _properties.maxNbMsaaSamples = _getMaxUsableSampleCount();
    vkGetPhysicalDeviceProperties(_physicalDevice, &_physicalDeviceProperties);
    _properties.maxSamplerAnisotropy = _physicalDeviceProperties.limits.maxSamplerAnisotropy;

    _queueFamilyIndices = candidateIndices;
    _swapChainSupportDetails = candidateSwapChainSupportDetails;

    /*
    * Logical device
    */

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {
        _queueFamilyIndices.graphicsFamily.value(),
        _queueFamilyIndices.presentationFamily.value()
    };

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures2 deviceFeatures = {};
    deviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures.features.samplerAnisotropy = VK_TRUE;
    deviceFeatures.features.sampleRateShading = VK_FALSE;
    deviceFeatures.features.multiDrawIndirect = VK_TRUE;

    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicStateFeatures{};
    extendedDynamicStateFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT;
    extendedDynamicStateFeatures.extendedDynamicState = true;

    deviceFeatures.pNext = &extendedDynamicStateFeatures;

    VkDeviceCreateInfo logicalDeviceCreateInfo = {};
    logicalDeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    logicalDeviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    logicalDeviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    logicalDeviceCreateInfo.pNext = &deviceFeatures;  // Attached to pNext instead of pEnabledFeatures as exepected (see VkPhysicalDeviceFeatures2).

    logicalDeviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    logicalDeviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    // Device-specific validation layers are not used anymore. We set them regardless for backward compatibility
    logicalDeviceCreateInfo.enabledLayerCount = 0;
    if (enableValidationLayers) {
        logicalDeviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(additionalLayers.size());
        logicalDeviceCreateInfo.ppEnabledLayerNames = additionalLayers.data();
    }

    VK_CHECK(vkCreateDevice(_physicalDevice, &logicalDeviceCreateInfo, nullptr, &_device));

    vkGetDeviceQueue(_device, _queueFamilyIndices.graphicsFamily.value(), 0, &_graphicsQueue);
    vkGetDeviceQueue(_device, _queueFamilyIndices.presentationFamily.value(), 0, &_presentationQueue);

    /*
    * Swap chain
    */

    _createSwapChain();

    /*
    * Memory allocator for images and buffers
    */

    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.physicalDevice = _physicalDevice;
    allocatorInfo.device = _device;
    allocatorInfo.instance = _vulkan;
    vmaCreateAllocator(&allocatorInfo, &_allocator);
}

void VulkanInstance::cleanup()
{
    cleanupSwapChain();

    vkDeviceWaitIdle(_device);

    vmaDestroyAllocator(_allocator);

    vkDestroyDevice(_device, nullptr);
    _device = VK_NULL_HANDLE;

    if (_debugMessenger) {
        if (auto vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(_vulkan, "vkDestroyDebugUtilsMessengerEXT")) {
            vkDestroyDebugUtilsMessengerEXT(_vulkan, _debugMessenger, nullptr);
            _debugMessenger = VK_NULL_HANDLE;
        }
        else {
            throw VulkanRendererException("Failed to load extension function vkDestroyDebugUtilsMessengerEXT");
        }
    }

    vkDestroySurfaceKHR(_vulkan, _surface, nullptr);
    _surface = VK_NULL_HANDLE;

    vkDestroyInstance(_vulkan, nullptr);
    _vulkan = VK_NULL_HANDLE;
}

bool VulkanInstance::QueueFamilyIndices::hasMandatoryFamilies()
{
    return graphicsFamily.has_value() && presentationFamily.has_value();
}


void VulkanInstance::recreateSwapChain()
{
    // TODO: Get width and height from the application instead.
    // Minimization: we wait until the window is expanded
    int width = 0, height = 0;
    glfwGetFramebufferSize(_window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(_window, &width, &height);
        glfwWaitEvents();
    }

    VkExtent2D swapChainExtent{};
    swapChainExtent.width = static_cast<uint32_t>(width);
    swapChainExtent.height = static_cast<uint32_t>(height);
    _properties.swapChainExtent = swapChainExtent;

    _createSwapChain();
}

void VulkanInstance::cleanupSwapChain() {
    vkDeviceWaitIdle(_device);

    for (size_t i = 0; i < _swapChainImageViews.size(); i++) {
        vkDestroyImageView(_device, _swapChainImageViews[i], nullptr);
    }

    vkDestroySwapchainKHR(_device, _swapChain, nullptr);
}

int VulkanInstance::_ratePhysicalDevice(VkPhysicalDevice device, QueueFamilyIndices& indices, SwapChainSupportDetails& swapChainSupportDetails) {
    VkPhysicalDeviceProperties2 deviceProperties2;
    deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;

    VkPhysicalDeviceDepthStencilResolvePropertiesKHR depthResolveProperties{};
    depthResolveProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES_KHR;
    deviceProperties2.pNext = &depthResolveProperties;

    vkGetPhysicalDeviceProperties2(device, &deviceProperties2);

    VkPhysicalDeviceProperties& deviceProperties = deviceProperties2.properties;

    VkPhysicalDeviceFeatures2 deviceFeatures{};
    deviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicStateFeatures{};
    extendedDynamicStateFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT;
    deviceFeatures.pNext = &extendedDynamicStateFeatures;

    vkGetPhysicalDeviceFeatures2(device, &deviceFeatures);

    if (!deviceFeatures.features.geometryShader) {
        return 0;
    }

    if (!deviceFeatures.features.samplerAnisotropy) {
        return 0;
    }

    if (!deviceFeatures.features.multiDrawIndirect) {
        return 0;
    }

    if (!deviceFeatures.features.sampleRateShading) {
        return 0;
    }

    if (!(static_cast<VkPhysicalDeviceExtendedDynamicStateFeaturesEXT*>(deviceFeatures.pNext))->extendedDynamicState) {
        return 0;
    }

    if (!(depthResolveProperties.supportedDepthResolveModes & (VK_RESOLVE_MODE_SAMPLE_ZERO_BIT | VK_RESOLVE_MODE_MIN_BIT | VK_RESOLVE_MODE_MAX_BIT))) {
        return 0;
    }

    // Check for the queue families available on the device
    indices = _findRequiredQueueFamilies(device);
    if (!indices.hasMandatoryFamilies()) {
        return 0;
    }

    if (!_areDeviceRequiredExtensionsSupported(device)) {
        return 0;
    }

    bool swapChainAdequate = false;
    swapChainSupportDetails = _querySwapChainSupportDetails(device);
    if (swapChainSupportDetails.formats.empty() || swapChainSupportDetails.presentModes.empty()) {
        return 0;
    }

    int score = 0;

    // Discrete GPUs have a significant performance advantage
    if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1000;
    }

    // Maximum possible size of textures affects graphics quality
    score += deviceProperties.limits.maxImageDimension2D;

    return score;
}

VulkanInstance::QueueFamilyIndices VulkanInstance::_findRequiredQueueFamilies(VkPhysicalDevice device)
{
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, _surface, &presentSupport);
        if (presentSupport) {
            indices.presentationFamily = i;
        }
    }

    return indices;
}

bool VulkanInstance::_areDeviceRequiredExtensionsSupported(VkPhysicalDevice device) {
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    const char* extensionNotFound = nullptr;
    for (int i = 0; i < deviceExtensions.size(); ++i) {
        bool found = false;
        for (const VkExtensionProperties& availableExtension : availableExtensions) {
            if (!strcmp(availableExtension.extensionName, deviceExtensions[i])) {
                found = true;
                break;
            }
        }
        if (!found) {
            extensionNotFound = deviceExtensions[i];
            break;
        }
    }

    return !extensionNotFound;
}

VulkanInstance::SwapChainSupportDetails VulkanInstance::_querySwapChainSupportDetails(VkPhysicalDevice device) {
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, _surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, _surface, &formatCount, nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, _surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, _surface, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, _surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

void VulkanInstance::_createSwapChain()
{
    _swapChainSupportDetails = _querySwapChainSupportDetails(_physicalDevice);

    VkSurfaceFormatKHR surfaceFormat = _chooseSwapSurfaceFormat(_swapChainSupportDetails.formats);
    VkPresentModeKHR presentMode = _chooseSwapPresentMode(_swapChainSupportDetails.presentModes);
    VkExtent2D extent = _chooseSwapChainExtent(_swapChainSupportDetails.capabilities);

    // NOTE: +1 than the minimum is recommended
    uint32_t imageCount = _swapChainSupportDetails.capabilities.minImageCount + 1;
    if (_swapChainSupportDetails.capabilities.maxImageCount > 0 && imageCount > _swapChainSupportDetails.capabilities.maxImageCount) {
        imageCount = _swapChainSupportDetails.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapChainCreateInfo = {};
    swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapChainCreateInfo.surface = _surface;
    swapChainCreateInfo.minImageCount = imageCount;
    swapChainCreateInfo.imageFormat = surfaceFormat.format;
    swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapChainCreateInfo.imageExtent = extent;
    swapChainCreateInfo.imageArrayLayers = 1;
    swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueFamilyIndices[] = {
        _queueFamilyIndices.graphicsFamily.value(),
        _queueFamilyIndices.presentationFamily.value()
    };

    swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapChainCreateInfo.queueFamilyIndexCount = 0;
    swapChainCreateInfo.pQueueFamilyIndices = nullptr;

    swapChainCreateInfo.preTransform = _swapChainSupportDetails.capabilities.currentTransform;
    swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapChainCreateInfo.presentMode = presentMode;
    swapChainCreateInfo.clipped = VK_TRUE;
    swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

    VK_CHECK(vkCreateSwapchainKHR(_device, &swapChainCreateInfo, nullptr, &_swapChain));

    vkGetSwapchainImagesKHR(_device, _swapChain, &imageCount, nullptr);
    _swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(_device, _swapChain, &imageCount, _swapChainImages.data());

    _properties.swapChainImageFormat = surfaceFormat.format;
    _properties.swapChainExtent = extent;

    /*
    * Swap chain image views
    */

    _swapChainImageViews.resize(_swapChainImages.size());

    for (uint32_t i = 0; i < _swapChainImages.size(); i++) {
        createImageView(_swapChainImages[i], _properties.swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1, _swapChainImageViews[i]);
    }
}

VkSurfaceFormatKHR VulkanInstance::_chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

VkPresentModeKHR VulkanInstance::_chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanInstance::_chooseSwapChainExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }
    else {
        int width, height;
        // We cant use WIDTH and HEIGHT because they are in screen coordinates,
        // and the dimensions in pixel may be larger than these if the monitor has high DPI
        // glfwGetFramebufferSize gives us the size in pixels
        glfwGetFramebufferSize(_window, &width, &height);

        VkExtent2D extent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return extent;
    }
}

/*
* Props to Sascha Willems.
* https://github.com/SaschaWillems/Vulkan/tree/master/examples/dynamicuniformbuffer
*/
size_t VulkanInstance::padUniformBufferSize(size_t originalSize)
{
    // Calculate required alignment based on minimum device offset alignment
    size_t minUboAlignment = _physicalDeviceProperties.limits.minUniformBufferOffsetAlignment;
    size_t alignedSize = originalSize;
    if (minUboAlignment > 0) {
        alignedSize = (alignedSize + minUboAlignment - 1) & ~(minUboAlignment - 1);
    }
    return alignedSize;
}

void VulkanInstance::createImageView(
    VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels, VkImageView& imageView, uint32_t baseMipLevel) const
{
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = baseMipLevel;
    viewInfo.subresourceRange.levelCount = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VK_CHECK(vkCreateImageView(_device, &viewInfo, nullptr, &imageView));
}

VkSampleCountFlagBits VulkanInstance::_getMaxUsableSampleCount() const {
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(_physicalDevice, &physicalDeviceProperties);

    VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
    if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
    if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
    if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
    if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
    if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

    return VK_SAMPLE_COUNT_1_BIT;
}

void VulkanInstance::createImage(
    uint32_t width,
    uint32_t height,
    uint32_t mipLevels,
    VkSampleCountFlagBits numSamples,
    VkFormat format,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags properties,
    AllocatedImage& image)
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

    VmaAllocationCreateInfo vmaAllocInfo = {};
    vmaAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VK_CHECK(vmaCreateImage(_allocator, &imageInfo, &vmaAllocInfo, &image.image, &image.vmaAllocation, nullptr));

    image.mipLevels = mipLevels;
}

void VulkanInstance::copyDataToImage(VkCommandPool commandPool, uint32_t width, uint32_t height, uint32_t nbChannels,
    AllocatedImage& image, const void* data, VkImageAspectFlags aspect)
{
    AllocatedBuffer stagingBuffer;
    uint32_t imageSize = width * height * nbChannels;
    createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, stagingBuffer);

    copyDataToBuffer(imageSize, stagingBuffer, data, 0);

    copyBufferToImage(commandPool, stagingBuffer.buffer, image.image, width, height, aspect);

    destroyBuffer(stagingBuffer);
}

void VulkanInstance::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
    VmaMemoryUsage memoryUsage, AllocatedBuffer& buffer, uint32_t minAlignment)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo vmaAllocInfo = {};
    vmaAllocInfo.usage = memoryUsage;

    if (minAlignment) {
        VK_CHECK(vmaCreateBufferWithAlignment(_allocator, &bufferInfo, &vmaAllocInfo,
            minAlignment, &buffer.buffer, &buffer.vmaAllocation, nullptr));
    }
    else {
        VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &vmaAllocInfo, &buffer.buffer, &buffer.vmaAllocation, nullptr));
    }
}

void VulkanInstance::createGPUBufferFromCPUData(VkCommandPool cmdPool, VkDeviceSize size, VkBufferUsageFlags usage, const void* data, AllocatedBuffer& buffer)
{
    createBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage, VMA_MEMORY_USAGE_GPU_ONLY, buffer);

    if (data) {
        AllocatedBuffer stagingBuffer;
        createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, stagingBuffer);
        copyDataToBuffer(static_cast<uint32_t>(size), stagingBuffer, data);
        copyBufferToBuffer(cmdPool, stagingBuffer.buffer, buffer.buffer, size);
        destroyBuffer(stagingBuffer);
    }
}

void VulkanInstance::copyDataToBuffer(uint32_t size, AllocatedBuffer& buffer, const void *data, uint32_t offset)
{
    void* dstData = nullptr;
    VK_CHECK(vmaMapMemory(_allocator, buffer.vmaAllocation, &dstData));
    dstData = static_cast<char*>(dstData) + offset;
    memcpy(dstData, data, size);
    vmaUnmapMemory(_allocator, buffer.vmaAllocation);
}


void VulkanInstance::copyBufferToBuffer(VkCommandPool cmdPool, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(cmdPool);

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    endSingleTimeCommands(commandBuffer, cmdPool);
}

void VulkanInstance::copyBufferToImage(VkCommandPool cmdPool, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, VkImageAspectFlags aspect) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(cmdPool);

    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = aspect;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { width, height, 1 };

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    endSingleTimeCommands(commandBuffer, cmdPool);
}

void VulkanInstance::destroyBuffer(AllocatedBuffer& buffer)
{
    vmaDestroyBuffer(_allocator, buffer.buffer, buffer.vmaAllocation);
    buffer = {};
}

void VulkanInstance::destroyImage(AllocatedImage& image)
{
    vmaDestroyImage(_allocator, image.image, image.vmaAllocation);
    image = {};
}

void* VulkanInstance::mapBuffer(AllocatedBuffer& buffer)
{
    void* data = nullptr;
    VK_CHECK(vmaMapMemory(_allocator, buffer.vmaAllocation, &data));
    return data;
}

void VulkanInstance::unmapBuffer(AllocatedBuffer& buffer)
{
    vmaUnmapMemory(_allocator, buffer.vmaAllocation);
}

void VulkanInstance::generateMipmaps(VkCommandPool cmdPool, AllocatedImage& imageData, VkFormat imageFormat, int32_t texWidth, int32_t texHeight) {
    // Check if image format supports linear blitting
    findSupportedFormat({ imageFormat }, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT);

    VkCommandBuffer commandBuffer = beginSingleTimeCommands(cmdPool);

    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = imageData.image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int32_t mipWidth = texWidth;
    int32_t mipHeight = texHeight;

    for (uint32_t i = 1; i < imageData.mipLevels; i++) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        VkImageBlit blit = {};
        blit.srcOffsets[0] = { 0, 0, 0 };
        blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = { 0, 0, 0 };
        blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(commandBuffer, imageData.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, imageData.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    barrier.subresourceRange.baseMipLevel = imageData.mipLevels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

    endSingleTimeCommands(commandBuffer, cmdPool);
}

VkCommandBuffer VulkanInstance::beginSingleTimeCommands(VkCommandPool& commandPool) {
    VkCommandBufferAllocateInfo allocInfo = VulkanUtils::createCommandBufferAllocateInfo(commandPool, 1);

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(_device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void VulkanInstance::endSingleTimeCommands(VkCommandBuffer commandBuffer, VkCommandPool& commandPool) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(_graphicsQueue);

    vkFreeCommandBuffers(_device, commandPool, 1, &commandBuffer);
}

VkFormat VulkanInstance::findSupportedFormat(const std::vector<VkFormat>& candidates,
    VkImageTiling tiling, VkFormatFeatureFlags features) const
{
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(_physicalDevice, format, &props);
        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw VulkanRendererException("Failed to find supported format.");
    return VkFormat::VK_FORMAT_UNDEFINED;
}

const std::vector<VkImage>& VulkanInstance::getSwapChainImages() const
{
    return _swapChainImages;
}

std::vector<VkImage>& VulkanInstance::getSwapChainImages()
{
    return _swapChainImages;
}

uint32_t VulkanInstance::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(_physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw VulkanRendererException("Failed to find suitable memory type.");
}

VkPhysicalDevice& VulkanInstance::getPhysicalDevice()
{
    return _physicalDevice;
}

VkInstance& VulkanInstance::getInstance()
{
    return _vulkan;
}

const VulkanInstance::Properties& VulkanInstance::getProperties() const {
    return _properties;
}

VkDevice& VulkanInstance::getLogicalDevice() {
    return _device;
}

const VulkanInstance::QueueFamilyIndices& VulkanInstance::getQueueFamilyIndices() const {
    return _queueFamilyIndices;
}

const std::vector<VkImageView>& VulkanInstance::getSwapChainImageViews() const
{
    return _swapChainImageViews;
}

VkQueue& VulkanInstance::getGraphicsQueue()
{
    return _graphicsQueue;
}

VkQueue& VulkanInstance::getPresentationQueue()
{
    return _presentationQueue;
}

VkSwapchainKHR& VulkanInstance::getSwapChain() {
    return _swapChain;
}

size_t VulkanInstance::getSwapChainSize() const {
    return _swapChainImages.size();
}

namespace {
    void getRequiredInstanceExtensionsNames(std::vector<const char*>& requiredExtensions) {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        requiredExtensions = std::vector<const char*>(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationLayers) {
            for (const char* extensionName : debugInstanceExtensions) {
                requiredExtensions.push_back(extensionName);
            }
        }
    }

    bool checkRequiredInstanceExtensionsSupport(const std::vector<const char*>& requiredExtensions) {
        uint32_t extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

        // Check for GLFW extensions support
        const char* extensionNotFound = nullptr;
        for (const char* requiredExtensionName : requiredExtensions) {
            bool found = false;
            for (const VkExtensionProperties& availableExtension : availableExtensions) {
                if (!strcmp(availableExtension.extensionName, requiredExtensionName)) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                extensionNotFound = requiredExtensionName;
                break;
            }
        }
        if (extensionNotFound) {
            throw VulkanRendererException((std::string("Required instance extension \"") + extensionNotFound + "\" not found.").c_str());
        }

        return true;
    }

    bool checkValidationLayerSupport(const std::vector<const char*>& requiredLayers) {
        bool result = true;
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        // Check for required validation layers support
        for (int i = 0; i < requiredLayers.size(); ++i) {
            bool found = false;
            for (const VkLayerProperties& layer : availableLayers) {
                if (!strcmp(layer.layerName, requiredLayers[i])) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                throw VulkanRendererException((std::string("Required validation layer ")+ requiredLayers[i] + " not found.").c_str());
            }
        }
        return result;
    }

    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData)
    {

        if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
            std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;
            return VK_FALSE;
        }

        return VK_FALSE;
    }
}
