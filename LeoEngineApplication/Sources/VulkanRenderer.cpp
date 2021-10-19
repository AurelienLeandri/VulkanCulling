#include "VulkanRenderer.h"

#include <Scene/Scene.h>
#include <Scene/Materials/PerformanceMaterial.h>
#include <Scene/Lights/DirectionalLight.h>
#include <Scene/Lights/PointLight.h>
#include <Scene/Geometries/Mesh.h>
#include <Scene/Transform.h>
#include <Scene/Camera.h>

#include "VulkanUtils.h"

#include <iostream>
#include <array>
#include <fstream>

#include <glm/gtc/matrix_transform.hpp>

VulkanRenderer::VulkanRenderer(VulkanInstance* vulkan, Options options) :
	_vulkan(vulkan), _options(options), _device(vulkan->getLogicalDevice())
{
    _framesData.resize(_vulkan->getSwapChainSize());
}

VulkanRenderer::~VulkanRenderer()
{
    if (_cleanup()) {
        std::cerr << "Error: Cleanup of Vulkan renderer entirely of partially failed." << std::endl;
    }
}

int VulkanRenderer::_cleanup()
{
    vkDeviceWaitIdle(_device);

    _cleanupSwapChainDependentResources();

    // Cleanup of the images
    for (auto& entry : _materialsImages) {
        for (auto& imageData : entry.second) {
            vkDestroyImage(_device, imageData.image, nullptr);
            vkFreeMemory(_device, imageData.memory, nullptr);
            vkDestroySampler(_device, imageData.textureSampler, nullptr);
            vkDestroyImageView(_device, imageData.view, nullptr);
        }
        entry.second.clear();
    }
    _materialsImages.clear();

    // Cleanup of the geometry buffers
    _objectsPerMaterial.clear();
    for (RenderableObject& object : _renderableObjects) {
        vkDestroyBuffer(_device, object.vertexBuffer.buffer, nullptr);
        vkFreeMemory(_device, object.vertexBuffer.deviceMemory, nullptr);
        vkDestroyBuffer(_device, object.indexBuffer.buffer, nullptr);
        vkFreeMemory(_device, object.indexBuffer.deviceMemory, nullptr);
    }
    _renderableObjects.clear();

    vkDestroyDescriptorSetLayout(_device, _materialDescriptorSetLayout, nullptr);
    _materialDescriptorSetLayout = VK_NULL_HANDLE;

    vkDestroyDescriptorSetLayout(_device, _sceneDataDescriptorSetLayout, nullptr);
    _sceneDataDescriptorSetLayout = VK_NULL_HANDLE;

    return 0;
}


void VulkanRenderer::_cleanupSwapChainDependentResources()
{
    // TODO: See more precisely what can be reused in case of resize. There must be a more elegant way than recreating all these objects.

    vkDeviceWaitIdle(_device);

    // Destroy the graphics pipeline
    vkDestroyPipeline(_device, _graphicsPipeline, nullptr);
    _graphicsPipeline = VK_NULL_HANDLE;
    vkDestroyPipelineLayout(_device, _pipelineLayout, nullptr);
    _pipelineLayout = VK_NULL_HANDLE;
    vkDestroyRenderPass(_device, _renderPass, nullptr);
    _renderPass = VK_NULL_HANDLE;

    // Destroy the descriptor sets
    vkDestroyDescriptorPool(_device, _descriptorPool, nullptr);
    _descriptorPool = VK_NULL_HANDLE;
    _materialDescriptorSets.clear();

    vkDestroyCommandPool(_device, _mainCommandPool, nullptr);

    // Destroying UBOs
    for (FrameData& frameData : _framesData) {
        vkDestroyBuffer(_device, frameData.cameraBuffer.buffer, nullptr);
        vkFreeMemory(_device, frameData.cameraBuffer.deviceMemory, nullptr);
        vkDestroyFramebuffer(_device, frameData.framebuffer, nullptr);
        vkFreeCommandBuffers(_device, frameData.commandPool, 1, &frameData.commandBuffer);
        vkDestroyCommandPool(_device, frameData.commandPool, nullptr);
        vkDestroySemaphore(_device, frameData.presentSemaphore, nullptr);
        vkDestroySemaphore(_device, frameData.renderSemaphore, nullptr);
        vkDestroyFence(_device, frameData.renderFinishedFence, nullptr);
    }
    _framesData.clear();

    vkDestroyBuffer(_device, _sceneDataBuffer.buffer, nullptr);
    vkFreeMemory(_device, _sceneDataBuffer.deviceMemory, nullptr);

    // Cleanup of the framebuffers shared data
    vkDestroyImageView(_device, _framebufferColor.view, nullptr);
    vkDestroyImage(_device, _framebufferColor.image, nullptr);
    vkFreeMemory(_device, _framebufferColor.memory, nullptr);
    _framebufferColor = {};

    vkDestroyImageView(_device, _framebufferDepth.view, nullptr);
    vkDestroyImage(_device, _framebufferDepth.image, nullptr);
    vkFreeMemory(_device, _framebufferDepth.memory, nullptr);
    _framebufferDepth = {};
}

int VulkanRenderer::init()
{
    _constructSceneRelatedStructures();

    _depthBufferFormat = _vulkan->findSupportedFormat(
        { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
    if (_depthBufferFormat == VK_FORMAT_UNDEFINED) {
        std::cerr << "Failed to find a supported format for the depth buffer." << std::endl;
        return -1;
    }

    if (_createRenderPass()) {
        std::cerr << "Could not create render pass" << std::endl;
        return -1;
    }

    if (_createDescriptorSetLayouts()) {
        std::cerr << "Could not create descriptor set layout." << std::endl;
        return -1;
    }

    if (_createGraphicsPipeline()) {
        std::cerr << "Could not create graphics pipeline." << std::endl;
        return -1;
    }

    if (_createCommandPools()) {
        std::cerr << "Could not initialize command pool." << std::endl;
        return -1;
    }

    if (_createFramebufferImageResources() || _createFramebuffers()) {
        std::cerr << "Failed to create framebuffers" << std::endl;
        return -1;
    }

    if (_createBuffers()) {
        std::cerr << "Could not initialize input buffers." << std::endl;
        return -1;
    }

    if (_loadImagesToDeviceMemory()) {
        std::cerr << "Could not initialize input images." << std::endl;
        return -1;
    }

    if (_createDescriptorPools() || _createDescriptorSets()) {
        std::cerr << "Could not create descriptors data." << std::endl;
        return -1;
    }

    if (_createCommandBuffers()) {
        std::cerr << "Could not create and setup drawing command buffers." << std::endl;
        return -1;
    }

    if (_createSyncObjects()) {
        std::cerr << "Could not create and setup fences and semaphores for synchronising rendering operation in the swap chain." << std::endl;
        return -1;
    }

	return 0;
}

int VulkanRenderer::iterate()
{
    FrameData& frameData = _framesData[_currentFrame];

    vkWaitForFences(_device, 1, &frameData.renderFinishedFence, VK_TRUE, UINT64_MAX);
    vkResetFences(_device, 1, &frameData.renderFinishedFence);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(
        _device, _vulkan->getSwapChain(), UINT64_MAX, frameData.presentSemaphore, VK_NULL_HANDLE, &imageIndex);

    // Swap chain is invalid or suboptimal (for example because of a window resize)
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        _cleanupSwapChainDependentResources();
        _vulkan->recreateSwapChain();
        _recreateSwapChainDependentResources();
        return 0;
    }
    else if (result && result != VK_SUBOPTIMAL_KHR) {
        std::cerr << "Failed to acquire swap chain image." << std::endl;
        return -1;
    }

    _updateFrameLevelUniformBuffers(imageIndex);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { frameData.presentSemaphore };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &_framesData[imageIndex].commandBuffer;
    VkSemaphore signalSemaphores[] = { frameData.renderSemaphore };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;


    /*
    * Recording drawing commands
    */

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;
    beginInfo.pInheritanceInfo = nullptr;

    if (vkBeginCommandBuffer(_framesData[imageIndex].commandBuffer, &beginInfo)) {
        std::cerr << "Failed to begin recording command buffer." << std::endl;
        return -1;
    }

    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = _renderPass;
    renderPassInfo.framebuffer = _framesData[imageIndex].framebuffer;
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = _vulkan->getProperties().swapChainExtent;

    std::array<VkClearValue, 2> clearValues = {};
    clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
    clearValues[1].depthStencil = { 1.0f, 0 };
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(_framesData[imageIndex].commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(_framesData[imageIndex].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline);

    size_t materialIndex = 0;
    for (auto& entry : _objectsPerMaterial) {
        const leo::Material* material = entry.first;
        VkDeviceSize offsets[] = { 0 };

        for (const RenderableObject* object : entry.second) {
            vkCmdBindVertexBuffers(_framesData[imageIndex].commandBuffer, 0, 1, &object->vertexBuffer.buffer, offsets);

            vkCmdBindIndexBuffer(_framesData[imageIndex].commandBuffer, object->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

            std::array<VkDescriptorSet, 2> descriptorSets = {
                _framesData[imageIndex].globalDataDescriptorSet,
                _materialDescriptorSets[imageIndex * _objectsPerMaterial.size() + materialIndex],
            };
            vkCmdBindDescriptorSets(_framesData[imageIndex].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                _pipelineLayout, 0, 2, descriptorSets.data(), 0, nullptr);

            vkCmdDrawIndexed(_framesData[imageIndex].commandBuffer, static_cast<uint32_t>(object->nbElements), 1, 0, 0, 0);
        }

        materialIndex++;
    }

    vkCmdEndRenderPass(_framesData[imageIndex].commandBuffer);

    if (vkEndCommandBuffer(_framesData[imageIndex].commandBuffer)) {
        std::cerr << "Failed to record command buffer." << std::endl;
        return -1;
    }

    if (vkQueueSubmit(_vulkan->getGraphicsQueue(), 1, &submitInfo, frameData.renderFinishedFence) != VK_SUCCESS) {
        std::cerr << "Failed to submit draw command buffer." << std::endl;
        return -1;
    }

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = { _vulkan->getSwapChain() };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr;

    result = vkQueuePresentKHR(_vulkan->getPresentationQueue(), &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || InputManager::framebufferResized) {
        InputManager::framebufferResized = false;
        _cleanupSwapChainDependentResources();
        _vulkan->recreateSwapChain();
        _recreateSwapChainDependentResources();
    }
    else if (result) {
        std::cerr << "Failed to present swap chain image." << std::endl;
        return -1;
    }

    _currentFrame = (_currentFrame + 1) % _MAX_FRAMES_IN_FLIGHT;

    return 0;
}

int VulkanRenderer::_recreateSwapChainDependentResources() {
    if (_createRenderPass()) {
        std::cerr << "Could not create render pass" << std::endl;
        return -1;
    }

    if (_createGraphicsPipeline()) {
        std::cerr << "Could not create graphics pipeline." << std::endl;
        return -1;
    }

    if (_createFramebufferImageResources()) {
        std::cerr << "Failed to create framebuffers" << std::endl;
        return -1;
    }

    if (_createFramebuffers()) {
        std::cerr << "Failed to create framebuffers" << std::endl;
        return -1;
    }

    if (_createBuffers()) {
        std::cerr << "Could not initialize input buffers." << std::endl;
        return -1;
    }

    if (_createDescriptorPools()) {
        std::cerr << "Could not create descriptors data." << std::endl;
        return -1;
    }

    if (_createDescriptorSets()) {
        std::cerr << "Could not create descriptors data." << std::endl;
        return -1;
    }

    if (_createCommandBuffers()) {
        std::cerr << "Could not create and setup drawing command buffers." << std::endl;
        return -1;
    }

    return 0;
}


int VulkanRenderer::_createCommandPools()
{
    const VulkanInstance::QueueFamilyIndices& queueFamilyIndices = _vulkan->getQueueFamilyIndices();

    VkCommandPoolCreateInfo poolInfo = VulkanUtils::createCommandPoolInfo(queueFamilyIndices.graphicsFamily.value(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    // TODO: maybe make a command pool for transfer operations only, for switching layouts, transfering data and stuff.
    if (vkCreateCommandPool(_device, &poolInfo, nullptr, &_mainCommandPool)) {
        std::cerr << "Failed to create command pool." << std::endl;
        return -1;
    }

    for (FrameData& frameData : _framesData) {
        if (vkCreateCommandPool(_device, &poolInfo, nullptr, &frameData.commandPool)) {
            std::cerr << "Failed to create command pool." << std::endl;
            return -1;
        }
    }

    return 0;
}

int VulkanRenderer::_createBuffers()
{
    /*
    * Vertex and index buffers
    */

    for (RenderableObject& object : _renderableObjects) {
        const leo::Material* material = object.material;
        if (object.sceneShape->getType() == leo::Shape::Type::MESH) {
            const leo::Mesh* mesh = static_cast<const leo::Mesh*>(object.sceneShape);

            /*
            * Vertex buffer
            */

            const std::vector<leo::Vertex>& vertices = mesh->vertices;

            VkDeviceSize bufferSize = sizeof(leo::Vertex) * vertices.size();

            VkBuffer stagingBuffer;
            VkDeviceMemory stagingBufferMemory;
            if (_createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                stagingBuffer, stagingBufferMemory))
            {
                return -1;
            }

            void* data = nullptr;
            vkMapMemory(_device, stagingBufferMemory, 0, bufferSize, 0, &data);
            memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
            vkUnmapMemory(_device, stagingBufferMemory);

            if (_createBuffer(bufferSize,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                object.vertexBuffer.buffer, object.vertexBuffer.deviceMemory))
            {
                return -1;
            }

            _copyBuffer(stagingBuffer, object.vertexBuffer.buffer, bufferSize);

            vkDestroyBuffer(_device, stagingBuffer, nullptr);
            stagingBuffer = VK_NULL_HANDLE;
            vkFreeMemory(_device, stagingBufferMemory, nullptr);
            stagingBufferMemory = VK_NULL_HANDLE;

            /*
            * Index buffer
            */

            const std::vector<uint32_t>& indices = mesh->indices;
            VkDeviceSize indicesBufferSize = sizeof(indices[0]) * indices.size();

            object.nbElements = indices.size();

            VkBuffer indexStagingBuffer;
            VkDeviceMemory indexStagingBufferMemory;
            if (_createBuffer(indicesBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, indexStagingBuffer, indexStagingBufferMemory))
            {
                return -1;
            }

            void* indexData = nullptr;
            vkMapMemory(_device, indexStagingBufferMemory, 0, indicesBufferSize, 0, &indexData);
            memcpy(indexData, indices.data(), static_cast<size_t>(indicesBufferSize));
            vkUnmapMemory(_device, indexStagingBufferMemory);

            if (_createBuffer(indicesBufferSize,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, object.indexBuffer.buffer, object.indexBuffer.deviceMemory))
            {
                return -1;
            }

            _copyBuffer(indexStagingBuffer, object.indexBuffer.buffer, indicesBufferSize);

            vkDestroyBuffer(_device, indexStagingBuffer, nullptr);
            indexStagingBuffer = VK_NULL_HANDLE;
            vkFreeMemory(_device, indexStagingBufferMemory, nullptr);
            indexStagingBufferMemory = VK_NULL_HANDLE;
        }
        else {
            // TODO: Spheres and single triangles?
        }
    }

    /*
    * Creating camera buffers
    */

    size_t nbSwapChainImages = _vulkan->getSwapChainImageViews().size();
    VkDeviceSize cameraBufferSize = sizeof (GPUCameraData);

    for (size_t i = 0; i < nbSwapChainImages; i++) {
        if (_createBuffer(cameraBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            _framesData[i].cameraBuffer.buffer, _framesData[i].cameraBuffer.deviceMemory))
        {
            std::cerr << "Could not create uniform buffer" << std::endl;
            return -1;
        }
    }

    /*
    * Creating Scene data buffer
    */
    size_t bufferSize = _MAX_FRAMES_IN_FLIGHT * _vulkan->padUniformBufferSize(sizeof (GPUSceneData));
    if (_createBuffer(cameraBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        _sceneDataBuffer.buffer, _sceneDataBuffer.deviceMemory))
    {
        std::cerr << "Could not create uniform buffer" << std::endl;
        return -1;
    }

    return 0;
}

void VulkanRenderer::_updateFrameLevelUniformBuffers(uint32_t currentImage) {
    /*
    * Camera data
    */

    GPUCameraData cameraData;
    cameraData.view = glm::lookAt(_camera->getPosition(), _camera->getPosition() + _camera->getFront(), _camera->getUp());
    // TODO: Real values for model
    /*
    ubo.model = glm::mat4(0.01f);
    ubo.model[3][3] = 1;
    ubo.model[1][1] *= -1;
    */
    const VkExtent2D& swapChainExtent = _vulkan->getProperties().swapChainExtent;
    cameraData.proj = glm::perspective(glm::radians(45.0f), static_cast<float>(swapChainExtent.width) / swapChainExtent.height, 0.1f, 100000.0f);
    cameraData.viewProj = cameraData.proj * cameraData.view;

    void* data = nullptr;
    vkMapMemory(_device, _framesData[currentImage].cameraBuffer.deviceMemory, 0, sizeof (GPUCameraData), 0, &data);
    memcpy(data, &cameraData, sizeof (GPUCameraData));
    vkUnmapMemory(_device, _framesData[currentImage].cameraBuffer.deviceMemory);

    /*
    * Scene data
    */

    GPUSceneData sceneData;
    sceneData.ambientColor = { 1, 0, 0, 0 };
    sceneData.sunlightColor = { 0, 1, 0, 0 };
    sceneData.sunlightDirection = { 0, 0, 0, 1 };

    data = nullptr;
    vkMapMemory(_device, _sceneDataBuffer.deviceMemory, 0, sizeof (GPUSceneData), 0, &data);
    memcpy(data, &sceneData, sizeof (GPUSceneData));
    vkUnmapMemory(_device, _sceneDataBuffer.deviceMemory);
}

int VulkanRenderer::_createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(_device, &bufferInfo, nullptr, &buffer)) {
        std::cerr << "Failed to create buffer." << std::endl;
        return -1;
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(_device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = _vulkan->findMemoryType(memRequirements.memoryTypeBits, properties);

    // TODO: This should not be called for every resource but instead we should use offsets
    // and put several buffers into one allocation.
    if (vkAllocateMemory(_device, &allocInfo, nullptr, &bufferMemory)) {
        std::cerr << "Failed to allocate buffer memory." << std::endl;
        return -1;
    }

    vkBindBufferMemory(_device, buffer, bufferMemory, 0);

    return 0;
}

int VulkanRenderer::_copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBuffer commandBuffer = _beginSingleTimeCommands(_mainCommandPool);

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    _endSingleTimeCommands(commandBuffer, _mainCommandPool);

    return 0;
}

int VulkanRenderer::_loadImagesToDeviceMemory() {
    for (auto& entry : _materialsImages) {
        const leo::PerformanceMaterial* material = static_cast<const leo::PerformanceMaterial*>(entry.first);
        std::vector<_ImageData>& vulkanImages = entry.second;

        static const size_t nbTexturesInMaterial = 5;
        std::array<const leo::ImageTexture*, nbTexturesInMaterial> materialTextures = {
            material->diffuseTexture.get(), material->specularTexture.get(), material->ambientTexture.get(), material->normalsTexture.get(), material->heightTexture.get()
        };

        for (size_t i = 0; i < nbTexturesInMaterial; ++i) {
            _ImageData& vulkanImageData = vulkanImages[i];
            const leo::ImageTexture* materialTexture = materialTextures[i];

            uint32_t texWidth = static_cast<uint32_t>(materialTexture->width);
            uint32_t texHeight = static_cast<uint32_t>(materialTexture->height);

            vulkanImageData.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

            uint32_t nbChannels = 0;
            VkFormat imageFormat = VkFormat::VK_FORMAT_UNDEFINED;
            switch (materialTexture->layout) {
            case leo::ImageTexture::Layout::R:
                imageFormat = VK_FORMAT_R8_UNORM;
                nbChannels = 1;
                break;
            case leo::ImageTexture::Layout::RGBA:
                if (i == 3) { // Normals texture
                    imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
                }
                else {
                    imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
                }
                nbChannels = 4;
                break;
            default:
                break;
            }

            if (!nbChannels || imageFormat == VkFormat::VK_FORMAT_UNDEFINED) {
                std::cerr << "A texture on a material has a format that is not expected. Something is very very wrong." << std::endl;
                return -1;
            }

            // Image handle and memory

            if (_vulkan->createImage(texWidth, texHeight, vulkanImageData.mipLevels, VK_SAMPLE_COUNT_1_BIT, imageFormat, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vulkanImageData.image, vulkanImageData.memory))
            {
                std::cerr << "Failed to create image." << std::endl;
                return -1;
            }

            if (_transitionImageLayout(vulkanImageData, imageFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)) {
                std::cerr << "Error during image layout transition." << std::endl;
                return -1;
            }

            VkBuffer stagingBuffer;
            VkDeviceMemory stagingBufferMemory;
            VkDeviceSize imageSize = texWidth * texHeight * nbChannels;
            _createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

            void* data = nullptr;
            vkMapMemory(_device, stagingBufferMemory, 0, imageSize, 0, &data);
            memcpy(data, materialTexture->data, static_cast<size_t>(imageSize));
            vkUnmapMemory(_device, stagingBufferMemory);

            _copyBufferToImage(stagingBuffer, vulkanImageData.image, texWidth, texHeight);

            _generateMipmaps(vulkanImageData, imageFormat, texWidth, texHeight);

            vkDestroyBuffer(_device, stagingBuffer, nullptr);
            stagingBuffer = VK_NULL_HANDLE;
            vkFreeMemory(_device, stagingBufferMemory, nullptr);
            stagingBufferMemory = VK_NULL_HANDLE;

            // Image view

            if (_vulkan->createImageView(vulkanImageData.image, imageFormat, VK_IMAGE_ASPECT_COLOR_BIT, vulkanImageData.mipLevels, vulkanImageData.view)) {
                std::cerr << "Could not create image view for material texture." << std::endl;
                return -1;
            }

            // Texture sampler

            VkSamplerCreateInfo samplerInfo = {};
            samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerInfo.magFilter = VK_FILTER_LINEAR;
            samplerInfo.minFilter = VK_FILTER_LINEAR;
            samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.anisotropyEnable = VK_TRUE;

            samplerInfo.maxAnisotropy = _vulkan->getProperties().maxSamplerAnisotropy;

            samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
            samplerInfo.unnormalizedCoordinates = VK_FALSE;
            samplerInfo.compareEnable = VK_FALSE;
            samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

            samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            samplerInfo.minLod = 0.0f;
            samplerInfo.maxLod = static_cast<float>(vulkanImageData.mipLevels);
            samplerInfo.mipLodBias = 0.0f;

            if (vkCreateSampler(_device, &samplerInfo, nullptr, &vulkanImageData.textureSampler)) {
                std::cerr << "Failed to create texture sampler." << std::endl;
                return -1;
            }
        }
    }

    return 0;
}

int VulkanRenderer::_transitionImageLayout(_ImageData& imageData, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkCommandBuffer commandBuffer = _beginSingleTimeCommands(_mainCommandPool);

    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = imageData.image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = imageData.mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = 0;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else {
        std::cerr << "Unsupported layout transition." << std::endl;
        return -1;
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    _endSingleTimeCommands(commandBuffer, _mainCommandPool);

    return 0;
}

void VulkanRenderer::_copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    // TODO: Try to setup a command buffer to record several operations and then flush everything once,
    // instead of calling beginSingleTimeCommands and endSingleTimeCommands each time.
    // Could also be done for uniforms and other stuff that creates single-time command buffers several times.
    // For example pass the commandBuffer as parameter and initialize it before doing all the createBuffer/Image, copyBuffer/image stuff.
    VkCommandBuffer commandBuffer = _beginSingleTimeCommands(_mainCommandPool);

    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { width, height, 1 };

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    _endSingleTimeCommands(commandBuffer, _mainCommandPool);
}

void VulkanRenderer::_generateMipmaps(_ImageData& imageData, VkFormat imageFormat, int32_t texWidth, int32_t texHeight) {
    // Check if image format supports linear blitting
    _vulkan->findSupportedFormat({ imageFormat }, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT);

    VkCommandBuffer commandBuffer = _beginSingleTimeCommands(_mainCommandPool);

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

    _endSingleTimeCommands(commandBuffer, _mainCommandPool);
}

int VulkanRenderer::_createGraphicsPipeline()
{
    /*
    * Shaders
    */

    VkShaderModule vertexShaderModule;
    if (_createShaderModule("../Resources/Shaders/vert.spv", vertexShaderModule)) {
        return -1;
    }

    VkShaderModule fragmentShaderModule;
    if (_createShaderModule("../Resources/Shaders/frag.spv", fragmentShaderModule)) {
        return -1;
    }

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertexShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragmentShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    /*
    * Fixed-function stages
    */

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkVertexInputBindingDescription bindingDescription = {};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof (leo::Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof (leo::Vertex, position);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof (leo::Vertex, normal);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset = offsetof (leo::Vertex, uv);

    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    const VulkanInstance::Properties& instanceProperties = _vulkan->getProperties();

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(instanceProperties.swapChainExtent.width);
    viewport.height = static_cast<float>(instanceProperties.swapChainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = instanceProperties.swapChainExtent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f;
    rasterizer.depthBiasClamp = 0.0f;
    rasterizer.depthBiasSlopeFactor = 0.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = instanceProperties.maxNbMsaaSamples;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;
    // Antialiazing on shading, for example textures with sudden color changes.
    // Has a performance cost.
    multisampling.sampleShadingEnable = VK_TRUE;
    multisampling.minSampleShading = .2f; // min fraction for sample shading; closer to one is smooth

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    /*
    * Creating the pipeline
    */

    VkDynamicState dynamicStates[] = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_LINE_WIDTH
    };

    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    std::array<VkDescriptorSetLayout, 2> pSetLayouts = { _sceneDataDescriptorSetLayout, _materialDescriptorSetLayout };
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(pSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = pSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    if (vkCreatePipelineLayout(_device, &pipelineLayoutInfo, nullptr, &_pipelineLayout)) {
        std::cerr << "Failed to create pipeline layout." << std::endl;
        return -1;
    }

    VkPipelineDepthStencilStateCreateInfo depthStencil = VulkanUtils::createDepthStencilCreateInfo(true, true, VK_COMPARE_OP_LESS);

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = nullptr; // Optional
    pipelineInfo.layout = _pipelineLayout;
    pipelineInfo.renderPass = _renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
    pipelineInfo.basePipelineIndex = -1; // Optional

    VkDevice& device = _device;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_graphicsPipeline)) {
        std::cerr << "Failed to create graphics pipeline." << std::endl;
        return -1;
    }

    vkDestroyShaderModule(device, fragmentShaderModule, nullptr);
    fragmentShaderModule = VK_NULL_HANDLE;
    vkDestroyShaderModule(device, vertexShaderModule, nullptr);
    vertexShaderModule = VK_NULL_HANDLE;

    return 0;
}

int VulkanRenderer::_createShaderModule(const char* glslFilePath, VkShaderModule& shaderModule)
{
    std::ifstream file(glslFilePath, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        std::cerr << "Failed to open spir-v file " << "\"" << glslFilePath << "\"." << std::endl;
        return -1;
    }

    std::vector<char> buffer((size_t)file.tellg());
    file.seekg(0);
    file.read(buffer.data(), buffer.size());

    file.close();

    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = buffer.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(buffer.data());

    if (vkCreateShaderModule(_device, &createInfo, nullptr, &shaderModule)) {
        std::cerr << "Failed to create shader module." << std::endl;
        return -1;
    }

    return 0;
}

void VulkanRenderer::_constructSceneRelatedStructures()
{
    for (const leo::SceneObject& sceneObject : _scene->objects) {
        const leo::Material* material = sceneObject.material.get();
        const leo::Shape* shape = sceneObject.shape.get();
        const leo::Transform* transform = sceneObject.transform.get();  // No per scene object transform. No point in that since the scene doesnt move. We must pre-transform objects.
        RenderableObject object = {};
        object.material = material;
        object.sceneShape = shape;
        _renderableObjects.push_back(object);
    }
    for (RenderableObject& object : _renderableObjects) {
        if (_objectsPerMaterial.find(object.material) == _objectsPerMaterial.end()) {
            _objectsPerMaterial[object.material] = std::vector<RenderableObject*>();
        }
        if (_materialsImages.find(object.material) == _materialsImages.end()) {
            _materialsImages[object.material] = std::vector<_ImageData>(5);
        }
        _objectsPerMaterial[object.material].push_back(&object);
    }
}

int VulkanRenderer::_createDescriptorSetLayouts()
{
    /*
    * Camera transforms
    */

    VkDescriptorSetLayoutBinding uboTransformsLayoutBinding = {};
    uboTransformsLayoutBinding.binding = 0;
    uboTransformsLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboTransformsLayoutBinding.descriptorCount = 1;
    uboTransformsLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    uboTransformsLayoutBinding.pImmutableSamplers = nullptr;

    /*
    * Scene data
    */

    VkDescriptorSetLayoutBinding sceneDataLayoutBinding = {};
    sceneDataLayoutBinding.binding = 1;
    sceneDataLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sceneDataLayoutBinding.descriptorCount = 1;
    sceneDataLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    sceneDataLayoutBinding.pImmutableSamplers = nullptr;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = { uboTransformsLayoutBinding, sceneDataLayoutBinding };

    VkDescriptorSetLayoutCreateInfo setLayoutInfo = {};
    setLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    setLayoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    setLayoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(_device, &setLayoutInfo, nullptr, &_sceneDataDescriptorSetLayout)) {
        std::cerr << "Failed to create transforms descriptor set layout." << std::endl;
        return -1;
    }

    /*
    * Material layout
    */

    static const uint32_t nbTexturesInMaterial = 5;

    std::array<VkDescriptorSetLayoutBinding, nbTexturesInMaterial> materialBindings = { {} };

    for (uint32_t i = 0; i < nbTexturesInMaterial; ++i) {
        materialBindings[i].binding = i;
        materialBindings[i].descriptorCount = 1;
        materialBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        materialBindings[i].pImmutableSamplers = nullptr;
        materialBindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    VkDescriptorSetLayoutCreateInfo materialLayoutInfo{};
    materialLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    materialLayoutInfo.bindingCount = static_cast<uint32_t>(materialBindings.size());
    materialLayoutInfo.pBindings = materialBindings.data();

    if (vkCreateDescriptorSetLayout(_device, &materialLayoutInfo, nullptr, &_materialDescriptorSetLayout)) {
        std::cerr << "Failed to create material descriptor set layout." << std::endl;
        return -1;
    }

    return 0;
}

int VulkanRenderer::_createRenderPass()
{
    const VulkanInstance::Properties& instanceProperties = _vulkan->getProperties();

    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = instanceProperties.swapChainImageFormat;
    colorAttachment.samples = instanceProperties.maxNbMsaaSamples;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depthAttachment = {};
    depthAttachment.format = _depthBufferFormat;
    depthAttachment.samples = instanceProperties.maxNbMsaaSamples;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef = {};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription colorAttachmentResolve = {};
    colorAttachmentResolve.format = instanceProperties.swapChainImageFormat;
    colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentResolveRef = {};
    colorAttachmentResolveRef.attachment = 2;
    colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;
    subpass.pResolveAttachments = &colorAttachmentResolveRef;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 3> attachments = {
        colorAttachment,
        depthAttachment,
        colorAttachmentResolve
    };

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(_device, &renderPassInfo, nullptr, &_renderPass)) {
        std::cerr << "Failed to create render pass" << std::endl;
        return -1;
    }

    return 0;
}

int VulkanRenderer::_createFramebufferImageResources()
{
    const VulkanInstance::Properties& instanceProperties = _vulkan->getProperties();
    VkFormat colorFormat = _vulkan->getProperties().swapChainImageFormat;

    _vulkan->createImage(
        instanceProperties.swapChainExtent.width,
        instanceProperties.swapChainExtent.height,
        1,
        instanceProperties.maxNbMsaaSamples,
        colorFormat, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        _framebufferColor.image,
        _framebufferColor.memory);

    if (_vulkan->createImageView(_framebufferColor.image, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1, _framebufferColor.view)) {
        std::cerr << "Could not create framebuffer color image view." << std::endl;
        return -1;
    }

    _vulkan->createImage(
        instanceProperties.swapChainExtent.width,
        instanceProperties.swapChainExtent.height,
        1,
        instanceProperties.maxNbMsaaSamples,
        _depthBufferFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        _framebufferDepth.image,
        _framebufferDepth.memory);

    if (_vulkan->createImageView(_framebufferDepth.image, _depthBufferFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1, _framebufferDepth.view)) {
        std::cerr << "Could not create framebuffer depth image view." << std::endl;
        return -1;
    }

    return 0;
}

int VulkanRenderer::_createDescriptorPools()
{
    size_t swapChainSize = _vulkan->getSwapChainSize();
    std::vector<VkDescriptorPoolSize> poolSizes = {};
    poolSizes.push_back({});
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(swapChainSize);

    poolSizes.push_back({});
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(swapChainSize * _objectsPerMaterial.size()) * 5;  // 5 textures in the material

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(swapChainSize * (_objectsPerMaterial.size() + 1));  // One set per material plus one transform UBO, per swap chain image

    if (vkCreateDescriptorPool(_device, &poolInfo, nullptr, &_descriptorPool)) {
        std::cerr << "Failed to create descriptor pool." << std::endl;
        return -1;
    }

    return 0;
}

int VulkanRenderer::_createDescriptorSets()
{
    size_t swapChainSize = _vulkan->getSwapChainSize();

    // Materials

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = _descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(swapChainSize * _objectsPerMaterial.size());
    std::vector<VkDescriptorSetLayout> layouts(allocInfo.descriptorSetCount, _materialDescriptorSetLayout);
    allocInfo.pSetLayouts = layouts.data();

    _materialDescriptorSets.resize(allocInfo.descriptorSetCount);
    if (vkAllocateDescriptorSets(_device, &allocInfo, _materialDescriptorSets.data())) {
        std::cerr << "Failed to allocate descriptor sets." << std::endl;
        return -1;
    }

    std::vector<VkWriteDescriptorSet> materialsDescriptorWrites(swapChainSize * _objectsPerMaterial.size() * 5, VkWriteDescriptorSet());

    for (size_t i = 0; i < swapChainSize; ++i) {
        // Updating descriptor set for material textures
        // TODO: write once and copy the rest.
        size_t materialIdx = 0;
        for (const auto& entry : _objectsPerMaterial) {
            const leo::PerformanceMaterial* material = static_cast<const leo::PerformanceMaterial*>(entry.first);
            const std::vector<_ImageData>& materialImages = _materialsImages[material];

            VkDescriptorImageInfo imageInfo = {};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = materialImages[0].view;
            imageInfo.sampler = materialImages[0].textureSampler;

            for (size_t materialTextureIndex = 0; materialTextureIndex < 5; ++materialTextureIndex) {
                size_t descriptorSetIndex = i * _objectsPerMaterial.size() * 5 + materialIdx * 5 + materialTextureIndex;
                materialsDescriptorWrites[descriptorSetIndex].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                materialsDescriptorWrites[descriptorSetIndex].dstSet = _materialDescriptorSets[i * _objectsPerMaterial.size() + materialIdx];
                materialsDescriptorWrites[descriptorSetIndex].dstBinding = static_cast<uint32_t>(materialTextureIndex);
                materialsDescriptorWrites[descriptorSetIndex].dstArrayElement = 0;
                materialsDescriptorWrites[descriptorSetIndex].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                materialsDescriptorWrites[descriptorSetIndex].descriptorCount = 1;
                materialsDescriptorWrites[descriptorSetIndex].pImageInfo = &imageInfo;
            }

            materialIdx++;
        }

    }

    vkUpdateDescriptorSets(_device, static_cast<uint32_t>(materialsDescriptorWrites.size()), materialsDescriptorWrites.data(), 0, nullptr);

    /*
    * Creating descriptor sets
    */
    
    std::vector<VkWriteDescriptorSet> descriptorWrites(swapChainSize * 2, VkWriteDescriptorSet());
    std::vector<VkDescriptorBufferInfo> bufferInfos(swapChainSize * 2, VkDescriptorBufferInfo());

    for (size_t frameNumber = 0; frameNumber < swapChainSize; ++frameNumber) {
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = _descriptorPool;
        allocInfo.descriptorSetCount = 1;

        allocInfo.pSetLayouts = &_sceneDataDescriptorSetLayout;

        if (vkAllocateDescriptorSets(_device, &allocInfo, &_framesData[frameNumber].globalDataDescriptorSet)) {
            std::cerr << "Failed to allocate descriptor sets." << std::endl;
            return -1;
        }

        bufferInfos[frameNumber * 2].buffer = _framesData[frameNumber].cameraBuffer.buffer;
        bufferInfos[frameNumber * 2].offset = 0;
        bufferInfos[frameNumber * 2].range = sizeof (GPUCameraData);

        bufferInfos[frameNumber * 2 + 1].buffer = _sceneDataBuffer.buffer;
        bufferInfos[frameNumber * 2 + 1].offset = 0;
        bufferInfos[frameNumber * 2 + 1].range = sizeof (GPUSceneData);

        descriptorWrites[frameNumber * 2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[frameNumber * 2].dstSet = _framesData[frameNumber].globalDataDescriptorSet;
        descriptorWrites[frameNumber * 2].dstBinding = 0;
        descriptorWrites[frameNumber * 2].dstArrayElement = 0;
        descriptorWrites[frameNumber * 2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[frameNumber * 2].descriptorCount = 1;
        descriptorWrites[frameNumber * 2].pBufferInfo = &bufferInfos[frameNumber * 2];

        descriptorWrites[frameNumber * 2 + 1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[frameNumber * 2 + 1].dstSet = _framesData[frameNumber].globalDataDescriptorSet;
        descriptorWrites[frameNumber * 2 + 1].dstBinding = 1;
        descriptorWrites[frameNumber * 2 + 1].dstArrayElement = 0;
        descriptorWrites[frameNumber * 2 + 1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[frameNumber * 2 + 1].descriptorCount = 1;
        descriptorWrites[frameNumber * 2 + 1].pBufferInfo = &bufferInfos[frameNumber * 2 + 1];
    }

    vkUpdateDescriptorSets(_device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

    return 0;
}

int VulkanRenderer::_createFramebuffers()
{
    const std::vector<VkImageView>& swapChainImageViews = _vulkan->getSwapChainImageViews();

    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        std::array<VkImageView, 3> attachments = {
            _framebufferColor.view,
            _framebufferDepth.view,
            swapChainImageViews[i],
        };

        const VulkanInstance::Properties& instanceProperties = _vulkan->getProperties();

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = _renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = instanceProperties.swapChainExtent.width;
        framebufferInfo.height = instanceProperties.swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(_device, &framebufferInfo, nullptr, &_framesData[i].framebuffer)) {
            std::cerr << "Failed to create framebuffer." << std::endl;
            return -1;
        }
    }
    return 0;
}

int VulkanRenderer::_createCommandBuffers() {
    for (FrameData& frame : _framesData) {
        VkCommandBufferAllocateInfo allocInfo = VulkanUtils::createCommandBufferAllocateInfo(frame.commandPool, 1);

        if (vkAllocateCommandBuffers(_device, &allocInfo, &frame.commandBuffer)) {
            std::cerr << "Failed to allocate command buffers." << std::endl;
            return -1;
        }
    }

    return 0;
}

int VulkanRenderer::_createSyncObjects() {
    for (FrameData& frameData : _framesData) {
        VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        if (vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &frameData.presentSemaphore) ||
            vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &frameData.renderSemaphore) ||
            vkCreateFence(_device, &fenceInfo, nullptr, &frameData.renderFinishedFence))
        {
            std::cerr << "Failed to create synchronization objects for a frame." << std::endl;
            return -1;
        }
    }

    return 0;
}


VkCommandBuffer VulkanRenderer::_beginSingleTimeCommands(VkCommandPool& commandPool) {
    VkCommandBufferAllocateInfo allocInfo = VulkanUtils::createCommandBufferAllocateInfo(commandPool, 1);

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(_device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void VulkanRenderer::_endSingleTimeCommands(VkCommandBuffer commandBuffer, VkCommandPool& commandPool) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(_vulkan->getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(_vulkan->getGraphicsQueue());

    vkFreeCommandBuffers(_vulkan->getLogicalDevice(), commandPool, 1, &commandBuffer);
}

void VulkanRenderer::setScene(const leo::Scene* scene)
{
	_scene = scene;
}

void VulkanRenderer::setCamera(const leo::Camera* camera)
{
    _camera = camera;
}
