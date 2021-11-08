#include "VulkanRenderer.h"

#include <Scene/Scene.h>
#include <Scene/Materials/PerformanceMaterial.h>
#include <Scene/Lights/DirectionalLight.h>
#include <Scene/Lights/PointLight.h>
#include <Scene/Geometries/Mesh.h>
#include <Scene/Transform.h>
#include <Scene/Camera.h>

#include "VulkanUtils.h"
#include "DebugUtils.h"

#include <iostream>
#include <array>
#include <fstream>
#include <set>

#include <glm/gtc/matrix_transform.hpp>

#include <stb_image.h>

VulkanRenderer::VulkanRenderer(VulkanInstance* vulkan, Options options) :
	_vulkan(vulkan), _options(options), _device(vulkan->getLogicalDevice()), _globalDescriptorAllocator(_device), _descriptorLayoutCache(_device), _shaderBuilder(_device)
{
    _framesData.resize(_vulkan->getSwapChainSize());
}

VulkanRenderer::~VulkanRenderer()
{
    _cleanup();
}

void VulkanRenderer::_cleanup()
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

    for (ObjectsBatch& batch : _objectsBatches) {
        vkDestroyBuffer(_device, batch.vertexBuffer.buffer, nullptr);
        vkFreeMemory(_device, batch.vertexBuffer.deviceMemory, nullptr);
        vkDestroyBuffer(_device, batch.indexBuffer.buffer, nullptr);
        vkFreeMemory(_device, batch.indexBuffer.deviceMemory, nullptr);
    }
    _objectsBatches.clear();

    vkDestroyBuffer(_device, _indirectCommandBuffer.buffer, nullptr);
    vkFreeMemory(_device, _indirectCommandBuffer.deviceMemory, nullptr);
    _indirectCommandBuffer = {};
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

    vkDestroyCommandPool(_device, _mainCommandPool, nullptr);

    // Destroying UBOs
    for (FrameData& frameData : _framesData) {
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
    _sceneDataBuffer = {};
    vkDestroyBuffer(_device, _cameraDataBuffer.buffer, nullptr);
    vkFreeMemory(_device, _cameraDataBuffer.deviceMemory, nullptr);
    _cameraDataBuffer = {};
    vkDestroyBuffer(_device, _objectsDataBuffer.buffer, nullptr);
    vkFreeMemory(_device, _objectsDataBuffer.deviceMemory, nullptr);
    _objectsDataBuffer = {};

    // Destroy descriptors
    _globalDescriptorAllocator.cleanup();
    _descriptorLayoutCache.cleanup();

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

void VulkanRenderer::init()
{
    _depthBufferFormat = _vulkan->findSupportedFormat(
        { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
    if (_depthBufferFormat == VK_FORMAT_UNDEFINED) {
        throw VulkanRendererException("Failed to find a supported format for the depth buffer.");
    }

    _createCommandPools();

    _constructSceneRelatedStructures();
    _createInputBuffers();
    _createInputImages();
    _createDescriptors();
    _createRenderPass();
    _createGraphicsPipeline();
    _createFramebufferImageResources();
    _createFramebuffers();
    _createCommandBuffers();
    _createSyncObjects();
}

void VulkanRenderer::iterate()
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
    }
    else if (result && result != VK_SUBOPTIMAL_KHR) {
        throw VulkanRendererException("Failed to acquire swap chain image.");
    }

    _updateFrameLevelUniformBuffers(static_cast<uint32_t>(_currentFrame));

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

    VK_CHECK(vkBeginCommandBuffer(_framesData[imageIndex].commandBuffer, &beginInfo));

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

    // Global data descriptor set
    uint32_t uniformOffset = static_cast<uint32_t>(_vulkan->padUniformBufferSize(sizeof(GPUCameraData)) * _currentFrame);
    vkCmdBindDescriptorSets(_framesData[imageIndex].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        _pipelineLayout, 0, 1, &_globalDataDescriptorSet, 1, &uniformOffset);

    vkCmdBindDescriptorSets(_framesData[imageIndex].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        _pipelineLayout, 3, 1, &_testTextureDescriptorSet, 0, nullptr);

    uint32_t objectIndex = 0;
    uint32_t offset = 0;
    uint32_t stride = sizeof(VkDrawIndexedIndirectCommand);
    for (const ObjectsBatch& batch : _objectsBatches) {
        const leo::Material* material = batch.material;
        VkDeviceSize offsets[] = { 0 };

        // Materials data descriptor set
        vkCmdBindDescriptorSets(_framesData[imageIndex].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            _pipelineLayout, 2, 1, &_materialDescriptorSets[material], 0, nullptr);

        // Objects data descriptor set
        vkCmdBindDescriptorSets(_framesData[imageIndex].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            _pipelineLayout, 1, 1, &_objectsDataDescriptorSet, 0, nullptr);

        vkCmdBindVertexBuffers(_framesData[imageIndex].commandBuffer, 0, 1, &batch.vertexBuffer.buffer, offsets);

        vkCmdBindIndexBuffer(_framesData[imageIndex].commandBuffer, batch.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexedIndirect(_framesData[imageIndex].commandBuffer, _indirectCommandBuffer.buffer, offset, 1, stride);

        objectIndex++;
        offset += stride;
    }

    vkCmdEndRenderPass(_framesData[imageIndex].commandBuffer);

    VK_CHECK(vkEndCommandBuffer(_framesData[imageIndex].commandBuffer));

    VK_CHECK(vkQueueSubmit(_vulkan->getGraphicsQueue(), 1, &submitInfo, frameData.renderFinishedFence));

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
        throw VulkanRendererException("Failed to present swap chain image.");
    }

    _currentFrame = (_currentFrame + 1) % _MAX_FRAMES_IN_FLIGHT;
}

void VulkanRenderer::_recreateSwapChainDependentResources() {
    _createRenderPass();
    _createDescriptors();
    _createGraphicsPipeline();
    _createFramebufferImageResources();
    _createFramebuffers();
    _createInputBuffers();
    _createCommandBuffers();
}


void VulkanRenderer::_createCommandPools()
{
    const VulkanInstance::QueueFamilyIndices& queueFamilyIndices = _vulkan->getQueueFamilyIndices();

    VkCommandPoolCreateInfo poolInfo = VulkanUtils::createCommandPoolInfo(queueFamilyIndices.graphicsFamily.value(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    // TODO: maybe make a command pool for transfer operations only, for switching layouts, transfering data and stuff.
    VK_CHECK(vkCreateCommandPool(_device, &poolInfo, nullptr, &_mainCommandPool));

    for (FrameData& frameData : _framesData) {
        VK_CHECK(vkCreateCommandPool(_device, &poolInfo, nullptr, &frameData.commandPool));
    }
}

void VulkanRenderer::_createInputBuffers()
{
    /*
    * Creating camera buffers
    */

    size_t nbSwapChainImages = _vulkan->getSwapChainImageViews().size();
    VkDeviceSize cameraBufferSize = nbSwapChainImages * _vulkan->padUniformBufferSize(sizeof(GPUCameraData));

    _createBuffer(cameraBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        _cameraDataBuffer.buffer, _cameraDataBuffer.deviceMemory);

    /*
    * Creating scene data buffer
    */

    size_t sceneDataBufferSize = sizeof (GPUSceneData);
    _createBuffer(sceneDataBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        _sceneDataBuffer.buffer, _sceneDataBuffer.deviceMemory);

    // TODO: Put actual values (maybe from options and/or leo::Scene)
    GPUSceneData sceneData;
    sceneData.ambientColor = { 1, 0, 0, 0 };
    sceneData.sunlightColor = { 0, 1, 0, 0 };
    sceneData.sunlightDirection = { 0, 0, 0, 1 };

    void* data = nullptr;
    vkMapMemory(_device, _sceneDataBuffer.deviceMemory, 0, sizeof(GPUSceneData), 0, &data);
    memcpy(data, &sceneData, sizeof(GPUSceneData));
    vkUnmapMemory(_device, _sceneDataBuffer.deviceMemory);

    /*
    * Per-object data
    */

    size_t objectsDataBufferSize = _MAX_NUMBER_OBJECTS * sizeof(GPUObjectData);
    _createBuffer(objectsDataBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        _objectsDataBuffer.buffer, _objectsDataBuffer.deviceMemory);

    data = nullptr;
    vkMapMemory(_device, _objectsDataBuffer.deviceMemory, 0, objectsDataBufferSize, 0, &data);
    GPUObjectData* objectDataPtr = static_cast<GPUObjectData*>(data);

    for (const ObjectsBatch& batch : _objectsBatches) {
        objectDataPtr->modelMatrix = glm::mat4(0.01f);
        objectDataPtr->modelMatrix[1][1] *= -1;
        objectDataPtr->modelMatrix[3][3] = 1;
        objectDataPtr++;
    }

    vkUnmapMemory(_device, _objectsDataBuffer.deviceMemory);


    /*
    * Indirect Command buffer
    */

    std::vector<VkDrawIndexedIndirectCommand> commandBufferData(_objectsBatches.size(), VkDrawIndexedIndirectCommand{});
    for (int i = 0; i < _objectsBatches.size(); ++i) {
        VkDrawIndexedIndirectCommand& command = commandBufferData[i];
        const ObjectsBatch& batch = _objectsBatches[i];
        command.firstInstance = i;
        command.instanceCount = 1;
        command.indexCount = batch.primitivesPerObject;
    }
    _createGPUBuffer(commandBufferData.size() * sizeof(VkDrawIndexedIndirectCommand),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        commandBufferData.data(),
        _indirectCommandBuffer
    );
}

void VulkanRenderer::_updateFrameLevelUniformBuffers(uint32_t currentImage) {
    /*
    * Camera data
    */

    GPUCameraData cameraData = {};
    cameraData.view = glm::lookAt(_camera->getPosition(), _camera->getPosition() + _camera->getFront(), _camera->getUp());
    const VkExtent2D& swapChainExtent = _vulkan->getProperties().swapChainExtent;
    cameraData.proj = glm::perspective(glm::radians(45.0f), static_cast<float>(swapChainExtent.width) / swapChainExtent.height, 0.1f, 100000.0f);
    cameraData.viewProj = cameraData.proj * cameraData.view;

    void* data = nullptr;
    size_t cameraBufferPadding = _vulkan->padUniformBufferSize(sizeof(GPUCameraData));
    vkMapMemory(_device, _cameraDataBuffer.deviceMemory, cameraBufferPadding * currentImage, sizeof (GPUCameraData), 0, &data);
    memcpy(data, &cameraData, sizeof (GPUCameraData));
    vkUnmapMemory(_device, _cameraDataBuffer.deviceMemory);
}

void VulkanRenderer::_createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateBuffer(_device, &bufferInfo, nullptr, &buffer));

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(_device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = _vulkan->findMemoryType(memRequirements.memoryTypeBits, properties);

    // TODO: This should not be called for every resource but instead we should use offsets
    // and put several buffers into one allocation.
    VK_CHECK(vkAllocateMemory(_device, &allocInfo, nullptr, &bufferMemory));

    VK_CHECK(vkBindBufferMemory(_device, buffer, bufferMemory, 0));
}

void VulkanRenderer::_copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBuffer commandBuffer = _beginSingleTimeCommands(_mainCommandPool);

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    _endSingleTimeCommands(commandBuffer, _mainCommandPool);
}

void VulkanRenderer::_createGPUBuffer(VkDeviceSize size, VkBufferUsageFlags usage, const void* data, AllocatedBuffer& buffer)
{
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    _createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingBufferMemory);

    void* dstData = nullptr;
    vkMapMemory(_device, stagingBufferMemory, 0, size, 0, &dstData);
    memcpy(dstData, data, static_cast<size_t>(size));
    vkUnmapMemory(_device, stagingBufferMemory);

    _createBuffer(size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        buffer.buffer, buffer.deviceMemory);

    _copyBuffer(stagingBuffer, buffer.buffer, size);

    vkDestroyBuffer(_device, stagingBuffer, nullptr);
    stagingBuffer = VK_NULL_HANDLE;
    vkFreeMemory(_device, stagingBufferMemory, nullptr);
    stagingBufferMemory = VK_NULL_HANDLE;
}

void VulkanRenderer::_createInputImages() {
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
                throw VulkanRendererException("A texture on a material has a format that is not expected. Something is very very wrong.");
            }

            // Image handle and memory

            _vulkan->createImage(texWidth, texHeight, vulkanImageData.mipLevels, VK_SAMPLE_COUNT_1_BIT, imageFormat, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vulkanImageData.image, vulkanImageData.memory);

            _transitionImageLayout(vulkanImageData, imageFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            VkBuffer stagingBuffer;
            VkDeviceMemory stagingBufferMemory;
            VkDeviceSize imageSize = static_cast<uint64_t>(texWidth) * texHeight * nbChannels;
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

            _vulkan->createImageView(vulkanImageData.image, imageFormat, VK_IMAGE_ASPECT_COLOR_BIT, vulkanImageData.mipLevels, vulkanImageData.view);

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

            VK_CHECK(vkCreateSampler(_device, &samplerInfo, nullptr, &vulkanImageData.textureSampler));
        }
    }
}

void VulkanRenderer::_transitionImageLayout(_ImageData& imageData, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
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
        throw VulkanRendererException("Unsupported layout transition.");
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    _endSingleTimeCommands(commandBuffer, _mainCommandPool);
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

void VulkanRenderer::_createGraphicsPipeline()
{
    /*
    * Shaders
    */

    VkShaderModule vertexShaderModule, fragmentShaderModule;
    _shaderBuilder.createShaderModule("../Resources/Shaders/vert.spv", vertexShaderModule);
    _shaderBuilder.createShaderModule("../Resources/Shaders/frag.spv", fragmentShaderModule);

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
    std::array<VkDescriptorSetLayout, 4> pSetLayouts = { _globalDataDescriptorSetLayout, _objectsDataDescriptorSetLayout, _materialDescriptorSetLayout, _testDescriptorSetLayout };
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(pSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = pSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    VK_CHECK(vkCreatePipelineLayout(_device, &pipelineLayoutInfo, nullptr, &_pipelineLayout));

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
    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_graphicsPipeline));

    vkDestroyShaderModule(device, fragmentShaderModule, nullptr);
    fragmentShaderModule = VK_NULL_HANDLE;
    vkDestroyShaderModule(device, vertexShaderModule, nullptr);
    vertexShaderModule = VK_NULL_HANDLE;
}

void VulkanRenderer::_constructSceneRelatedStructures()
{
    // New impl
    std::map<const leo::Material*, std::map<const leo::Shape*, uint32_t>> nbObjectsPerBatch;
    for (const leo::SceneObject& sceneObject : _scene->objects) {
        const leo::Material* material = sceneObject.material.get();
        const leo::Shape* shape = sceneObject.shape.get();
        if (nbObjectsPerBatch.find(material) == nbObjectsPerBatch.end()) {
            nbObjectsPerBatch[material] = {};
        }
        if (nbObjectsPerBatch[material].find(shape) == nbObjectsPerBatch[material].end()) {
            nbObjectsPerBatch[material][shape] = 0;
        }
        nbObjectsPerBatch[material][shape]++;
    }
    _nbMaterials = nbObjectsPerBatch.size();

    for (const auto& materialShapesPair : nbObjectsPerBatch) {
        const leo::PerformanceMaterial* material = static_cast<const leo::PerformanceMaterial*>(materialShapesPair.first);  // TODO: assuming material is Performance for now
        for (const auto& shapeNbPair : materialShapesPair.second) {
            const leo::Mesh* mesh = static_cast<const leo::Mesh*>(shapeNbPair.first);  // TODO: assuming the shape is a mesh for now
            uint32_t nbObjects = shapeNbPair.second;
            AllocatedBuffer vertexBuffer{};
            AllocatedBuffer indexBuffer{};

            // Vertex buffer
            _createGPUBuffer(sizeof(leo::Vertex) * mesh->vertices.size(),
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, mesh->vertices.data(),
                vertexBuffer);

            // Index buffer
            _createGPUBuffer(sizeof(mesh->indices[0]) * mesh->indices.size(),
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT, mesh->indices.data(),
                indexBuffer);

            _objectsBatches.push_back({
                vertexBuffer,  // vertexBuffer
                indexBuffer,  // indexBuffer
                material,  // material
                mesh,  // mesh
                nbObjects,  // nbObjects
                static_cast<uint32_t>(mesh->indices.size()), // primitivesPerObject
                0,  // stride
                0,  // offset
                });
        }
    }

    for (ObjectsBatch& batch : _objectsBatches) {
        if (_materialsImages.find(batch.material) == _materialsImages.end()) {
            _materialsImages[batch.material] = std::vector<_ImageData>(5);
        }
    }
}

void VulkanRenderer::_createRenderPass()
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

    VK_CHECK(vkCreateRenderPass(_device, &renderPassInfo, nullptr, &_renderPass));
}

void VulkanRenderer::_createFramebufferImageResources()
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

    _vulkan->createImageView(_framebufferColor.image, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1, _framebufferColor.view);

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

    _vulkan->createImageView(_framebufferDepth.image, _depthBufferFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1, _framebufferDepth.view);
}

void VulkanRenderer::_createDescriptors()
{
    DescriptorAllocator::Options globalDescriptorAllocatorOptions = {};
    globalDescriptorAllocatorOptions.poolBaseSize = 10;
    globalDescriptorAllocatorOptions.poolSizes = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1.f },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1.f },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1.f },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, _nbMaterials * 5.f },
    };
    _globalDescriptorAllocator.init(globalDescriptorAllocatorOptions);

    DescriptorBuilder builder = DescriptorBuilder::begin(_device, _descriptorLayoutCache, _globalDescriptorAllocator);


    /*
    * Global data (set 0)
    */

    VkDescriptorBufferInfo cameraBufferInfo = {};
    cameraBufferInfo.buffer = _cameraDataBuffer.buffer;
    cameraBufferInfo.offset = 0;
    cameraBufferInfo.range = sizeof(GPUCameraData);

    VkDescriptorBufferInfo sceneBufferInfo = {};
    sceneBufferInfo.buffer = _sceneDataBuffer.buffer;
    sceneBufferInfo.offset = 0;
    sceneBufferInfo.range = sizeof(GPUSceneData);

    DescriptorBuilder::begin(_device, _descriptorLayoutCache, _globalDescriptorAllocator)
        .bindBuffer(0, cameraBufferInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT)
        .bindBuffer(1, sceneBufferInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(_globalDataDescriptorSet, _globalDataDescriptorSetLayout);

    /*
    * Per-object data (set 1)
    */

    VkDescriptorBufferInfo objectsDataBufferInfo = {};
    objectsDataBufferInfo.buffer = _objectsDataBuffer.buffer;
    objectsDataBufferInfo.offset = 0;
    objectsDataBufferInfo.range = _MAX_NUMBER_OBJECTS * sizeof(GPUObjectData);

    DescriptorBuilder::begin(_device, _descriptorLayoutCache, _globalDescriptorAllocator)
        .bindBuffer(0, objectsDataBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
        .build(_objectsDataDescriptorSet, _objectsDataDescriptorSetLayout);

    /*
    * Per-material data (set 2)
    */
    // TODO: Should be refactored completely once I have better material storage
    std::set<const leo::Material*> materialsSet;
    for (const ObjectsBatch& batch : _objectsBatches) {
        materialsSet.insert(batch.material);
    }
    for (const leo::Material* material : materialsSet) {
        _materialDescriptorSets[material] = VK_NULL_HANDLE;
        const std::vector<_ImageData>& images = _materialsImages[material];
        DescriptorBuilder builder = DescriptorBuilder::begin(_device, _descriptorLayoutCache, _globalDescriptorAllocator);
        std::array<VkDescriptorImageInfo, 5> imageInfos = { {} };
        for (int i = 0; i < 5; ++i) {
            imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfos[i].imageView = images[i].view;
            imageInfos[i].sampler = images[i].textureSampler;

            builder.bindImage(i, imageInfos[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        }

        builder.build(_materialDescriptorSets[material], _materialDescriptorSetLayout);
    }

    /*
    * Test texture
    */

    VkDescriptorImageInfo testImageInfo = {};
    testImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    testImageInfo.imageView = _materialsImages.begin()->second[3].view;
    testImageInfo.sampler = _materialsImages.begin()->second[3].textureSampler;

    DescriptorBuilder::begin(_device, _descriptorLayoutCache, _globalDescriptorAllocator)
        .bindImage(0, testImageInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(_testTextureDescriptorSet, _testDescriptorSetLayout);
}

void VulkanRenderer::_createFramebuffers()
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

        VK_CHECK(vkCreateFramebuffer(_device, &framebufferInfo, nullptr, &_framesData[i].framebuffer));
    }
}

void VulkanRenderer::_createCommandBuffers() {
    for (FrameData& frame : _framesData) {
        VkCommandBufferAllocateInfo allocInfo = VulkanUtils::createCommandBufferAllocateInfo(frame.commandPool, 1);
        VK_CHECK(vkAllocateCommandBuffers(_device, &allocInfo, &frame.commandBuffer));
    }
}

void VulkanRenderer::_createSyncObjects() {
    for (FrameData& frameData : _framesData) {
        VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VK_CHECK(vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &frameData.presentSemaphore));
        VK_CHECK(vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &frameData.renderSemaphore));
        VK_CHECK(vkCreateFence(_device, &fenceInfo, nullptr, &frameData.renderFinishedFence));
    }
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
