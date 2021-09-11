#include "VulkanInstance.h"

#include <exception>
#include <iostream>
#include <vector>
#include <set>
#include <algorithm>

namespace {
#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

    void getRequiredInstanceExtensions(std::vector<const char*>& requiredExtensions);
    void checkInstanceRequiredExtensionsSupport(const std::vector<const char*>& requiredExtensions);
    void checkValidationLayerSupport(const std::vector<const char*>& requiredLayers);
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

    VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);

    const std::vector<const char*> instanceExtensions = {
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    };

    const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
    };

    const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
}

bool VulkanInstance::QueueFamilyIndices::hasMandatoryFamilies()
{
    return graphicsFamily.has_value() && presentationFamily.has_value();
}

int VulkanInstance::init(GLFWwindow* window)
{
    _window = window;

    try {
        _createInstance();
        if (enableValidationLayers) _setupDebugMessenger();
        _createSurface();
        _pickPhysicalDevice();
        _createLogicalDevice();
        _createSwapChain();
        _createSwapChainImageViews();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return -1;
    }
    return 0;
}

void VulkanInstance::recreateSwapChain()
{
    // Minimization: we wait until the window is expanded
    int width = 0, height = 0;
    glfwGetFramebufferSize(_window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(_window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(_device);

    cleanupSwapChain();

    _createSwapChain();
    _createSwapChainImageViews();
}

int VulkanInstance::cleanup()
{
    cleanupSwapChain();

    vkDestroyDevice(_device, nullptr);

    if (_debugMessenger) {
        if (auto vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(_instance, "vkDestroyDebugUtilsMessengerEXT")) {
            vkDestroyDebugUtilsMessengerEXT(_instance, _debugMessenger, nullptr);
            _debugMessenger = VK_NULL_HANDLE;
        }
        else {
            std::cerr << "Failed to load extension function vkDestroyDebugUtilsMessengerEXT" << std::endl;
            return -1;
        }
    }

    vkDestroySurfaceKHR(_instance, _surface, nullptr);
    _surface = VK_NULL_HANDLE;
    vkDestroyInstance(_instance, nullptr);
    _instance = VK_NULL_HANDLE;

    return 0;
}

void VulkanInstance::waitForIdleDevice() const
{
    vkDeviceWaitIdle(_device);
}

void VulkanInstance::cleanupSwapChain() {
    for (size_t i = 0; i < _swapChainImageViews.size(); i++) {
        vkDestroyImageView(_device, _swapChainImageViews[i], nullptr);
    }

    vkDestroySwapchainKHR(_device, _swapChain, nullptr);
}

void VulkanInstance::_createInstance()
{
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "AurellEngine";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    // Get the extension from GLFW so that Vulkan can interface with the window system
    // TODO: store requiredExtensions as a "options" parameter or as member variable
    std::vector<const char*> requiredInstanceExtensions;
    getRequiredInstanceExtensions(requiredInstanceExtensions);
    checkInstanceRequiredExtensionsSupport(requiredInstanceExtensions);
    createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredInstanceExtensions.size());
    createInfo.ppEnabledExtensionNames = requiredInstanceExtensions.data();

    // Validation layers
    if (enableValidationLayers) {
        checkValidationLayerSupport(validationLayers);
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();

        // Create a debug messenger specificaly for instance creation and destruction.
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
    }

    if (vkCreateInstance(&createInfo, nullptr, &_instance)) {
        throw std::runtime_error("Failed to create instance!");
    }
}

void VulkanInstance::_setupDebugMessenger() {
    VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
    populateDebugMessengerCreateInfo(createInfo);

    if (auto vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(_instance, "vkCreateDebugUtilsMessengerEXT")) {
        if (vkCreateDebugUtilsMessengerEXT(_instance, &createInfo, nullptr, &_debugMessenger))
            throw std::runtime_error("Failed to create required VkDebugUtilsMessengerEXT");
    }
    else
        throw std::runtime_error("Failed to load extension function vkCreateDebugUtilsMessengerEXT");

}

void VulkanInstance::_createSurface() {
    if (glfwCreateWindowSurface(_instance, _window, nullptr, &_surface)) {
        throw std::runtime_error("Failed to create window surface!");
    }
}

void VulkanInstance::_pickPhysicalDevice()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(_instance, &deviceCount, nullptr);
    if (!deviceCount) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(_instance, &deviceCount, devices.data());

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
        throw std::runtime_error("Failed to find a suitable GPU!");
    }

    _physicalDevice = *candidateDevice;
    _properties.maxNbMsaaSamples = _getMaxUsableSampleCount();
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(_physicalDevice, &properties);
    _properties.maxSamplerAnisotropy = properties.limits.maxSamplerAnisotropy;

    _queueFamilyIndices = candidateIndices;
    _swapChainSupportDetails = candidateSwapChainSupportDetails;
}

int VulkanInstance::_ratePhysicalDevice(VkPhysicalDevice device, QueueFamilyIndices& indices, SwapChainSupportDetails& swapChainSupportDetails) {
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);

    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

    if (!deviceFeatures.geometryShader) {
        return 0;
    }

    if (!deviceFeatures.samplerAnisotropy) {
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
    swapChainSupportDetails = _querySwapChainSupport(device);
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

VulkanInstance::SwapChainSupportDetails VulkanInstance::_querySwapChainSupport(VkPhysicalDevice device) {
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


void VulkanInstance::_createLogicalDevice() {
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

    VkPhysicalDeviceFeatures deviceFeatures = {};
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    // Antialiazing on shading, for example textures with sudden color changes.
    // Has a performance cost.
    deviceFeatures.sampleRateShading = VK_TRUE;

    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pEnabledFeatures = &deviceFeatures;

    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    // Device-specific validation layers are not used anymore. We set them regardless for backward compatibility
    createInfo.enabledLayerCount = 0;
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    }

    if (vkCreateDevice(_physicalDevice, &createInfo, nullptr, &_device)) {
        throw std::runtime_error("Failed to create logical device!");
    }

    vkGetDeviceQueue(_device, _queueFamilyIndices.graphicsFamily.value(), 0, &_graphicsQueue);
    vkGetDeviceQueue(_device, _queueFamilyIndices.presentationFamily.value(), 0, &_presentationQueue);
}

void VulkanInstance::_createSwapChain() {
    _swapChainSupportDetails = _querySwapChainSupport(_physicalDevice);

    VkSurfaceFormatKHR surfaceFormat = _chooseSwapSurfaceFormat(_swapChainSupportDetails.formats);
    VkPresentModeKHR presentMode = _chooseSwapPresentMode(_swapChainSupportDetails.presentModes);
    VkExtent2D extent = _chooseSwapExtent(_swapChainSupportDetails.capabilities);

    // NOTE: +1 than the minimum is recommended
    uint32_t imageCount = _swapChainSupportDetails.capabilities.minImageCount + 1;
    if (_swapChainSupportDetails.capabilities.maxImageCount > 0 && imageCount > _swapChainSupportDetails.capabilities.maxImageCount) {
        imageCount = _swapChainSupportDetails.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = _surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueFamilyIndices[] = {
        _queueFamilyIndices.graphicsFamily.value(),
        _queueFamilyIndices.presentationFamily.value()
    };

    if (_queueFamilyIndices.graphicsFamily != _queueFamilyIndices.presentationFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }

    createInfo.preTransform = _swapChainSupportDetails.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(_device, &createInfo, nullptr, &_swapChain)) {
        throw std::runtime_error("Failed to create the swap chain");
    }

    vkGetSwapchainImagesKHR(_device, _swapChain, &imageCount, nullptr);
    _swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(_device, _swapChain, &imageCount, _swapChainImages.data());

    _properties.swapChainImageFormat = surfaceFormat.format;
    _properties.swapChainExtent = extent;
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
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanInstance::_chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
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

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

void VulkanInstance::_createSwapChainImageViews() {
    _swapChainImageViews.resize(_swapChainImages.size());

    for (uint32_t i = 0; i < _swapChainImages.size(); i++) {
        _swapChainImageViews[i] = createImageView(_swapChainImages[i], _properties.swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
    }
}

VkImageView VulkanInstance::createImageView(
    VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels) const
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
    if (vkCreateImageView(_device, &viewInfo, nullptr, &imageView)) {
        throw std::runtime_error("failed to create texture image view");
    }

    return imageView;
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

    throw std::runtime_error("Failed to find supported format");
}

uint32_t VulkanInstance::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(_physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type");
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
    void getRequiredInstanceExtensions(std::vector<const char*>& requiredExtensions) {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        requiredExtensions = std::vector<const char*>(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationLayers) {
            for (const char* extensionName : instanceExtensions) {
                requiredExtensions.push_back(extensionName);
            }
        }
    }

    void checkInstanceRequiredExtensionsSupport(const std::vector<const char*>& requiredExtensions) {
        uint32_t extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

        // Check for GLFW extensions support
        const char* extensionNotFound = nullptr;
        for (int i = 0; i < requiredExtensions.size(); ++i) {
            bool found = false;
            for (const VkExtensionProperties& availableExtension : availableExtensions) {
                if (!strcmp(availableExtension.extensionName, requiredExtensions[i])) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                extensionNotFound = requiredExtensions[i];
                break;
            }
        }
        if (extensionNotFound) {
            throw std::runtime_error(std::string("Required instance extension \"") + extensionNotFound + "\" not found");
        }
    }

    void checkValidationLayerSupport(const std::vector<const char*>& requiredLayers) {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        // Check for required validation layers support
        const char* layerNotFound = nullptr;
        for (int i = 0; i < requiredLayers.size(); ++i) {
            bool found = false;
            for (const VkLayerProperties& layer : availableLayers) {
                if (!strcmp(layer.layerName, requiredLayers[i])) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                layerNotFound = requiredLayers[i];
                break;
            }
        }
        if (layerNotFound) {
            throw std::runtime_error(std::string("Required validation layer \"") + layerNotFound + "\" not found");
        }
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
        }

        return VK_FALSE;
    }
}
