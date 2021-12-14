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
#include "Scene/Camera.h"

#include <iostream>
#include <array>
#include <fstream>
#include <set>

#include <GeometryIncludes.h>

#include <stb_image.h>

namespace {
    uint32_t previousPow2(uint32_t v);
}

VulkanRenderer::VulkanRenderer(VulkanInstance* vulkan, Options options) :
    _vulkan(vulkan),
    _options(options),
    _device(vulkan->getLogicalDevice()),
    _globalDescriptorAllocator(_device),
    _globalDescriptorLayoutCache(_device),
    _materialBuilder(_device, _vulkan),
    _shaderBuilder(_device),
    _cullingDescriptorAllocator(_device),
    _depthPyramidDescriptorAllocator(_device)
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
    _cullingDescriptorAllocator.cleanup();
    _globalDescriptorLayoutCache.cleanup();

    // Cleanup of the framebuffers shared data
    vkDestroyImageView(_device, _framebufferColor.view, nullptr);
    vkDestroyImage(_device, _framebufferColor.image, nullptr);
    vkFreeMemory(_device, _framebufferColor.memory, nullptr);
    _framebufferColor = {};

    vkDestroySampler(_device, _framebufferDepth.textureSampler, nullptr);
    vkDestroyImageView(_device, _framebufferDepth.view, nullptr);
    vkDestroyImage(_device, _framebufferDepth.image, nullptr);
    vkFreeMemory(_device, _framebufferDepth.memory, nullptr);
    _framebufferDepth = {};

    _materialBuilder.cleanup();
    for (std::unique_ptr<AllocatedImage>& imageData : _materialImagesData) {
        vkDestroySampler(_device, imageData->textureSampler, nullptr);
        vkDestroyImageView(_device, imageData->view, nullptr);
        vkDestroyImage(_device, imageData->image, nullptr);
        vkFreeMemory(_device, imageData->memory, nullptr);
    }
    _materialImagesData.clear();

    for (std::unique_ptr<ShapeData>& shapeData : _shapeData) {
        vkDestroyBuffer(_device, shapeData->vertexBuffer.buffer, nullptr);
        vkFreeMemory(_device, shapeData->vertexBuffer.deviceMemory, nullptr);
        vkDestroyBuffer(_device, shapeData->indexBuffer.buffer, nullptr);
        vkFreeMemory(_device, shapeData->indexBuffer.deviceMemory, nullptr);
    }
    _shapeData.clear();

    _objectsBatches.clear();

    vkDestroyBuffer(_device, _gpuBatches.buffer, nullptr);
    vkFreeMemory(_device, _gpuBatches.deviceMemory, nullptr);
    _gpuBatches = {};

    vkDestroyBuffer(_device, _gpuIndexToObjectId.buffer, nullptr);
    vkFreeMemory(_device, _gpuIndexToObjectId.deviceMemory, nullptr);
    _gpuIndexToObjectId = {};

    vkDestroyBuffer(_device, _gpuObjectEntries.buffer, nullptr);
    vkFreeMemory(_device, _gpuObjectEntries.deviceMemory, nullptr);
    _gpuObjectEntries = {};

    vkDestroyBuffer(_device, _gpuResetBatches.buffer, nullptr);
    vkFreeMemory(_device, _gpuResetBatches.deviceMemory, nullptr);
    _gpuResetBatches = {};

    vkDestroyBuffer(_device, _gpuCullingGlobalData.buffer, nullptr);
    vkFreeMemory(_device, _gpuCullingGlobalData.deviceMemory, nullptr);
    _gpuCullingGlobalData = {};


    _cullShaderPass.cleanup();
    vkDestroyPipeline(_device, _cullingPipeline, nullptr);
    _cullingPipeline = VK_NULL_HANDLE;
    vkDestroyPipelineLayout(_device, _cullingPipelineLayout, nullptr);
    _cullingPipelineLayout = VK_NULL_HANDLE;
}

void VulkanRenderer::init()
{

    _depthBufferFormat = _vulkan->findSupportedFormat(
        { VK_FORMAT_D32_SFLOAT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT
    );
    if (_depthBufferFormat == VK_FORMAT_UNDEFINED) {
        throw VulkanRendererException("Failed to find a supported format for the depth buffer.");
    }

    _createCommandPools();

    _createGlobalBuffers();
    _createFramebuffersImage();

    // TODO: Move to a function
    VkExtent2D swapChainExtent = _vulkan->getProperties().swapChainExtent;
    _vulkan->createImage(swapChainExtent.width, swapChainExtent.height, 1,
        VK_SAMPLE_COUNT_1_BIT,
        _depthBufferFormat,
        VK_IMAGE_TILING_OPTIMAL,
        // VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,  // ONE_SAMPLE
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,  // ONE_SAMPLE
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        _depthImage);

    _transitionImageLayout(_depthImage, _depthBufferFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);

    _vulkan->createImageView(_depthImage.image, _depthBufferFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1, _depthImage.view);
    _depthImage.mipLevels = 1;

    _createRenderPass();
    _createFramebuffers();

    _createCommandBuffers();
    _createSyncObjects();

    _materialBuilder.init({ _vulkan->getProperties().maxNbMsaaSamples, _renderPass });

    _createComputePipeline("../Resources/Shaders/indirect_cull.spv", _cullingPipeline, _cullingPipelineLayout, _cullShaderPass);

    _createOcclusionCullingData();

    _createComputePipeline("../Resources/Shaders/depth_pyramid.spv", _depthPyramidPipeline, _depthPyramidPipelineLayout, _depthPyramidShaderPass);

    _createDepthPyramidDescriptors();
}

void VulkanRenderer::iterate()
{
    FrameData& frameData = _framesData[_currentFrame];

    vkWaitForFences(_device, 1, &frameData.renderFinishedFence, VK_TRUE, UINT64_MAX);
    vkResetFences(_device, 1, &frameData.renderFinishedFence);

    /*
    static int times = 0;
    if (times < 350)
        times++;
    else {
        _testWriteDepthBufferToDisc();
        _testWriteDepthPyramidToDisc();
    }
    */

    _transitionImageLayout(_depthImage, _depthBufferFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(
        _device, _vulkan->getSwapChain(), UINT64_MAX, frameData.presentSemaphore, VK_NULL_HANDLE, &imageIndex);

    // Swap chain is invalid or suboptimal (for example because of a window resize)
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        throw VulkanRendererException("Failed to acquire swap chain image.");
    }

    _updateCamera(static_cast<uint32_t>(_currentFrame));

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
    * Recording commands
    */

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;
    beginInfo.pInheritanceInfo = nullptr;

    VK_CHECK(vkBeginCommandBuffer(_framesData[imageIndex].commandBuffer, &beginInfo));

    // Culling

    vkCmdBindPipeline(_framesData[imageIndex].commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _cullingPipeline);

    VkBufferCopy indirectCopy;
    indirectCopy.dstOffset = 0;
    indirectCopy.size = static_cast<uint32_t>(_objectsBatches.size() * sizeof(GPUBatch));
    indirectCopy.srcOffset = 0;
    vkCmdCopyBuffer(_framesData[imageIndex].commandBuffer, _gpuResetBatches.buffer, _gpuBatches.buffer, 1, &indirectCopy);

    vkCmdPipelineBarrier(_framesData[imageIndex].commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &_gpuBatchesResetBarrier, 0, nullptr);

    uint32_t uniformOffset = static_cast<uint32_t>(_vulkan->padUniformBufferSize(sizeof(GPUCameraData)) * _currentFrame);
    vkCmdBindDescriptorSets(_framesData[imageIndex].commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
        _cullingPipelineLayout, 0, 1, &_cullingDescriptorSet, 1, &uniformOffset);

    uint32_t groupCountX = static_cast<uint32_t>((_nbInstances / 256) + 1);
    vkCmdDispatch(_framesData[imageIndex].commandBuffer, groupCountX, 1, 1);

    std::array<VkBufferMemoryBarrier, 2> barriers = { _gpuIndexToObjectIdBarrier, _gpuBatchesBarrier };

    vkCmdPipelineBarrier(_framesData[imageIndex].commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0, 0, nullptr, barriers.size(), barriers.data(), 0, nullptr);

    // Drawing

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

    VkPipeline currentPipeline = _materialBuilder.getMaterialTemplate(MaterialType::BASIC)->getPipeline(ShaderPass::Type::FORWARD);
    VkPipelineLayout graphicsPipelineLayout = _materialBuilder.getMaterialTemplate(MaterialType::BASIC)->getPipelineLayout(ShaderPass::Type::FORWARD);
    vkCmdBindPipeline(_framesData[imageIndex].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, currentPipeline);

    // Global data descriptor set
    vkCmdBindDescriptorSets(_framesData[imageIndex].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        graphicsPipelineLayout, 0, 1, &_globalDataDescriptorSet, 1, &uniformOffset);

    uint32_t offset = 0;
    uint32_t stride = sizeof(GPUBatch);
    for (const ObjectsBatch& batch : _objectsBatches) {
        const Material* material = batch.material;

        VkPipeline materialPipeline = _materialBuilder.getMaterialTemplate(MaterialType::BASIC)->getPipeline(ShaderPass::Type::FORWARD);
        if (currentPipeline != materialPipeline) {
            currentPipeline = materialPipeline;
            graphicsPipelineLayout = _materialBuilder.getMaterialTemplate(MaterialType::BASIC)->getPipelineLayout(ShaderPass::Type::FORWARD);
            vkCmdBindPipeline(_framesData[imageIndex].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, currentPipeline);
        }

        VkDeviceSize offsets[] = { 0 };

        // Materials data descriptor set
        vkCmdBindDescriptorSets(_framesData[imageIndex].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            graphicsPipelineLayout, 2, 1, &material->getDescriptorSet(ShaderPass::Type::FORWARD), 0, nullptr);

        // Objects data descriptor set
        vkCmdBindDescriptorSets(_framesData[imageIndex].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            graphicsPipelineLayout, 1, 1, &_objectsDataDescriptorSet, 0, nullptr);

        vkCmdBindVertexBuffers(_framesData[imageIndex].commandBuffer, 0, 1, &batch.shape->vertexBuffer.buffer, offsets);

        vkCmdBindIndexBuffer(_framesData[imageIndex].commandBuffer, batch.shape->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexedIndirect(_framesData[imageIndex].commandBuffer, _gpuBatches.buffer, offset, 1, stride);

        offset += stride;
    }

    vkCmdEndRenderPass(_framesData[imageIndex].commandBuffer);

    // Depth pyramid building
    _computeDepthPyramid(_framesData[imageIndex].commandBuffer);  // ONE_SAMPLE

    VK_CHECK(vkEndCommandBuffer(_framesData[imageIndex].commandBuffer));

    VK_CHECK(vkQueueSubmit(_vulkan->getGraphicsQueue(), 1, &submitInfo, frameData.renderFinishedFence));
    {
        void* voidDataPtr = nullptr;
        GPUBatch* commandBufferPtr = nullptr;
        vkMapMemory(_device, _gpuBatches.deviceMemory, 0, _testBatchesSize * sizeof(GPUBatch), 0, &voidDataPtr);
        commandBufferPtr = static_cast<GPUBatch*>(voidDataPtr);
        for (int i = 0; i < _testBatchesSize; i++) {
            GPUBatch& batch = commandBufferPtr[i];
            int a = 0;
        }
        vkUnmapMemory(_device, _gpuBatches.deviceMemory);
    }

    /*
    * Presentation
    */

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
    if (result) {
        throw VulkanRendererException("Failed to present swap chain image.");
    }

    _currentFrame = (_currentFrame + 1) % _MAX_FRAMES_IN_FLIGHT;
}

void VulkanRenderer::_computeDepthPyramid(VkCommandBuffer commandBuffer) {
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &_framebufferDepthWriteBarrier);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _depthPyramidPipeline);

    for (int32_t i = 0; i < _depthPyramid.mipLevels; ++i)
    {
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _depthPyramidPipelineLayout, 0, 1, &_depthPyramidDescriptorSets[i], 0, nullptr);

        uint32_t levelWidth = glm::max(1u, _depthPyramidWidth >> i);
        uint32_t levelHeight = glm::max(1u, _depthPyramidHeight >> i);

        glm::vec2 levelSize(levelWidth, levelHeight);

        uint32_t groupCountX = (levelWidth + 32 - 1) / 32;
        uint32_t groupCountY = (levelHeight + 32 - 1) / 32;
        vkCmdPushConstants(commandBuffer, _depthPyramidPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(glm::vec2), &levelSize);
        vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &_depthPyramidMipLevelBarriers[i]);
    }

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &_framebufferDepthReadBarrier);
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

void VulkanRenderer::_createGlobalBuffers()
{
    /*
    * Creating camera buffers
    */

    size_t nbSwapChainImages = _vulkan->getSwapChainImageViews().size();
    VkDeviceSize cameraBufferSize = nbSwapChainImages * _vulkan->padUniformBufferSize(sizeof(GPUCameraData));

    _createBuffer(cameraBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        _cameraDataBuffer);

    /*
    * Creating scene data buffer
    */

    size_t sceneDataBufferSize = sizeof (GPUSceneData);
    _createBuffer(sceneDataBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        _sceneDataBuffer);
}

void VulkanRenderer::_updateCamera(uint32_t currentImage) {
    /*
    * Camera data
    */

    GPUCameraData cameraData = {};
    glm::vec3 front = _camera->getFront();
    glm::vec3 up = _camera->getUp();
    glm::vec3 position = _camera->getPosition();
    position.y *= -1;
    //position = glm::vec3(50, 50, 0);

    /*
    position = glm::vec3(0);
    up = glm::vec3(0, -1, 0);
    front = glm::vec3(0, 0, 1);
    glm::vec3 right(1, 0, 0);
    */

    cameraData.view = glm::lookAt(position, position + front, up);
    /*
    cameraData.view = glm::mat4(
        glm::vec4(1, 0, 0, 0),
        glm::vec4(0, -1, 0, 0),
        glm::vec4(0, 0, 1, 0),
        glm::vec4(0, 0, 0, 1)
    );
    */
    //cameraData.view[2] *= -1;
    //cameraData.view[0] *= -1;
    cameraData.proj = _projectionMatrix;
    cameraData.viewProj = cameraData.proj * cameraData.view;

    void* data = nullptr;
    size_t cameraBufferPadding = _vulkan->padUniformBufferSize(sizeof(GPUCameraData));
    vkMapMemory(_device, _cameraDataBuffer.deviceMemory, cameraBufferPadding * currentImage, sizeof (GPUCameraData), 0, &data);
    memcpy(data, &cameraData, sizeof (GPUCameraData));
    vkUnmapMemory(_device, _cameraDataBuffer.deviceMemory);
}

void VulkanRenderer::_createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties, AllocatedBuffer& buffer)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateBuffer(_device, &bufferInfo, nullptr, &buffer.buffer));

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(_device, buffer.buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = _vulkan->findMemoryType(memRequirements.memoryTypeBits, properties);

    // TODO: This should not be called for every resource but instead we should use offsets
    // and put several buffers into one allocation.
    VK_CHECK(vkAllocateMemory(_device, &allocInfo, nullptr, &buffer.deviceMemory));

    VK_CHECK(vkBindBufferMemory(_device, buffer.buffer, buffer.deviceMemory, 0));
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
    _createBuffer(size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        buffer);

    if (data) {
        AllocatedBuffer stagingBuffer;
        _createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingBuffer);

        void* dstData = nullptr;
        vkMapMemory(_device, stagingBuffer.deviceMemory, 0, size, 0, &dstData);
        memcpy(dstData, data, static_cast<size_t>(size));
        vkUnmapMemory(_device, stagingBuffer.deviceMemory);

        _copyBuffer(stagingBuffer.buffer, buffer.buffer, size);

        vkDestroyBuffer(_device, stagingBuffer.buffer, nullptr);
        vkFreeMemory(_device, stagingBuffer.deviceMemory, nullptr);
        stagingBuffer = {};
    }
}

void VulkanRenderer::_transitionImageLayout(AllocatedImage& imageData, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlags aspect)
{
    // TODO: Write a barrier helper and use it instead of this function.
    VkCommandBuffer commandBuffer = _beginSingleTimeCommands(_mainCommandPool);

    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = imageData.image;
    barrier.subresourceRange.aspectMask = aspect;
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
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else {
        throw VulkanRendererException("Unsupported layout transition.");
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    _endSingleTimeCommands(commandBuffer, _mainCommandPool);
}

void VulkanRenderer::_copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, VkImageAspectFlags aspect) {
    // TODO: Try to setup a command buffer to record several operations and then flush everything once,
    // instead of calling beginSingleTimeCommands and endSingleTimeCommands each time.
    // Could also be done for uniforms and other stuff that creates single-time command buffers several times.
    // For example pass the commandBuffer as parameter and initialize it before doing all the createBuffer/Image, copyBuffer/image stuff.
    VkCommandBuffer commandBuffer = _beginSingleTimeCommands(_mainCommandPool);

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

    _endSingleTimeCommands(commandBuffer, _mainCommandPool);
}

void VulkanRenderer::_generateMipmaps(AllocatedImage& imageData, VkFormat imageFormat, int32_t texWidth, int32_t texHeight) {
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

void VulkanRenderer::loadSceneToDevice(const leo::Scene* scene)
{
    std::map<const leo::Material*, Material*> loadedMaterialsCache;
    std::map<const leo::ImageTexture*, AllocatedImage*> loadedImagesCache;
    std::map<const leo::Shape*, ShapeData*> shapeDataCache;

    static struct _ObjectInstanceData {
        const leo::Shape* shape = nullptr;
        const leo::Transform* transform = nullptr;
    };
    std::map<const Material*, std::map<const ShapeData*, std::vector<_ObjectInstanceData>>> nbObjectsPerBatch;
    for (const leo::SceneObject& sceneObject : scene->objects) {
        const leo::PerformanceMaterial* sceneMaterial = static_cast<const leo::PerformanceMaterial*>(sceneObject.material.get());
        const leo::Shape* sceneShape = sceneObject.shape.get();
        Material* loadedMaterial = nullptr;
        ShapeData* loadedShape = nullptr;

        /*
        * Load material data on the device
        */
        if (loadedMaterialsCache.find(sceneMaterial) == loadedMaterialsCache.end()) {
            loadedMaterial = _materialBuilder.createMaterial(MaterialType::BASIC);

            static const size_t nbTexturesInMaterial = 5;
            std::array<const leo::ImageTexture*, nbTexturesInMaterial> materialTextures = {
                sceneMaterial->diffuseTexture.get(), sceneMaterial->specularTexture.get(), sceneMaterial->ambientTexture.get(), sceneMaterial->normalsTexture.get(), sceneMaterial->heightTexture.get()
            };

            for (size_t i = 0; i < nbTexturesInMaterial; ++i) {
                const leo::ImageTexture* sceneTexture = materialTextures[i];
                AllocatedImage* loadedImage = nullptr;
                if (loadedImagesCache.find(sceneTexture) == loadedImagesCache.end()) {
                    _materialImagesData.push_back(std::make_unique<AllocatedImage>());
                    loadedImage = _materialImagesData.back().get();

                    uint32_t texWidth = static_cast<uint32_t>(sceneTexture->width);
                    uint32_t texHeight = static_cast<uint32_t>(sceneTexture->height);

                    loadedImage->mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

                    uint32_t nbChannels = 0;
                    VkFormat imageFormat = VkFormat::VK_FORMAT_UNDEFINED;
                    switch (sceneTexture->layout) {
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
                        throw VulkanRendererException("A texture on a sceneMaterial has a format that is not expected. Something is very very wrong.");
                    }

                    // Image handle and memory

                    _vulkan->createImage(texWidth, texHeight, loadedImage->mipLevels, VK_SAMPLE_COUNT_1_BIT, imageFormat, VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, *loadedImage);

                    _transitionImageLayout(*loadedImage, imageFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

                    AllocatedBuffer stagingBuffer;
                    VkDeviceSize imageSize = static_cast<uint64_t>(texWidth) * texHeight * nbChannels;
                    _createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer);

                    void* data = nullptr;
                    vkMapMemory(_device, stagingBuffer.deviceMemory, 0, imageSize, 0, &data);
                    memcpy(data, sceneTexture->data, static_cast<size_t>(imageSize));
                    vkUnmapMemory(_device, stagingBuffer.deviceMemory);

                    _copyBufferToImage(stagingBuffer.buffer, loadedImage->image, texWidth, texHeight);

                    _generateMipmaps(*loadedImage, imageFormat, texWidth, texHeight);

                    vkDestroyBuffer(_device, stagingBuffer.buffer, nullptr);
                    vkFreeMemory(_device, stagingBuffer.deviceMemory, nullptr);
                    stagingBuffer = {};

                    // Image view

                    _vulkan->createImageView(loadedImage->image, imageFormat, VK_IMAGE_ASPECT_COLOR_BIT, loadedImage->mipLevels, loadedImage->view);

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
                    samplerInfo.maxLod = static_cast<float>(loadedImage->mipLevels);
                    samplerInfo.mipLodBias = 0.0f;

                    VK_CHECK(vkCreateSampler(_device, &samplerInfo, nullptr, &loadedImage->textureSampler));

                    loadedImagesCache[sceneTexture] = loadedImage;
                }
                else {
                    loadedImage = loadedImagesCache[sceneTexture];
                }

                loadedMaterial->textures[i].sampler = loadedImage->textureSampler;
                loadedMaterial->textures[i].view = loadedImage->view;
            }

            _materialBuilder.setupMaterialDescriptorSets(*loadedMaterial);

            loadedMaterialsCache[sceneMaterial] = loadedMaterial;
        }
        else {
            loadedMaterial = loadedMaterialsCache[sceneMaterial];
        }

        /*
        * Load shape data on the device
        */
        if (shapeDataCache.find(sceneShape) == shapeDataCache.end()) {
            _shapeData.push_back(std::make_unique<ShapeData>());
            loadedShape = _shapeData.back().get();

            const leo::Mesh* mesh = static_cast<const leo::Mesh*>(sceneShape);  // TODO: assuming the shape is a mesh for now

            // Vertex buffer
            _createGPUBuffer(sizeof(leo::Vertex) * mesh->vertices.size(),
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, mesh->vertices.data(),
                loadedShape->vertexBuffer);

            // Index buffer
            _createGPUBuffer(sizeof(mesh->indices[0]) * mesh->indices.size(),
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT, mesh->indices.data(),
                loadedShape->indexBuffer);

            loadedShape->nbElements = static_cast<uint32_t>(mesh->indices.size());

            shapeDataCache[sceneShape] = loadedShape;
        }
        else {
            loadedShape = shapeDataCache[sceneShape];
        }

        /*
        * Incrementing the counter for the given pair of material and shape data.
        */
        if (nbObjectsPerBatch.find(loadedMaterial) == nbObjectsPerBatch.end()) {
            nbObjectsPerBatch[loadedMaterial] = {};
        }
        if (nbObjectsPerBatch[loadedMaterial].find(loadedShape) == nbObjectsPerBatch[loadedMaterial].end()) {
            nbObjectsPerBatch[loadedMaterial][loadedShape] = {};
        }
        nbObjectsPerBatch[loadedMaterial][loadedShape].push_back({ sceneShape, sceneObject.transform.get() });
    }
    _nbMaterials = nbObjectsPerBatch.size();

    for (const auto& materialShapesPair : nbObjectsPerBatch) {
        const Material* material = materialShapesPair.first;  // TODO: assuming material is Performance for now
        for (const auto& shapeNbPair : materialShapesPair.second) {
            const ShapeData* shape = shapeNbPair.first;  // TODO: assuming the shape is a mesh for now

            _objectsBatches.push_back({
                material,  // material
                shape,  // shape
                static_cast<uint32_t>(shapeNbPair.second.size()),  // nbObjects
                shape->nbElements, // primitivesPerObject
                0,  // stride
                0,  // offset
                });
        }
    }

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

    size_t objectsDataBufferSize = scene->objects.size() * sizeof(GPUObjectData);

    std::vector<GPUObjectData> objectData(objectsDataBufferSize);
    int i = 0;
    for (const auto& materialShapesPair : nbObjectsPerBatch) {
        const Material* material = materialShapesPair.first;  // TODO: assuming material is Performance for now
        for (const auto& shapeNbPair : materialShapesPair.second) {
            const ShapeData* shape = shapeNbPair.first;  // TODO: assuming the shape is a mesh for now
            for (const _ObjectInstanceData& instanceData : shapeNbPair.second) {
                objectData[i].modelMatrix = instanceData.transform->getMatrix();
                const glm::vec4& sphereBounds = static_cast<const leo::Mesh*>(instanceData.shape)->boundingSphere;
                glm::vec4 b = instanceData.transform->getMatrix() * glm::vec4(1, 1, 1, 0);
                glm::vec4 a = glm::abs(b);
                float scale = glm::max(a.x, glm::max(a.y, a.z));
                glm::vec4 transformedSphere = instanceData.transform->getMatrix() * glm::vec4(sphereBounds.x, sphereBounds.y, sphereBounds.z, 1.0f);
                transformedSphere /= transformedSphere.w;
                transformedSphere.w = scale * sphereBounds.w;
                objectData[i].sphereBounds = transformedSphere;
                i++;
            }
        }
    }

    _createGPUBuffer(objectsDataBufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        objectData.data(),
        _objectsDataBuffer
    );

    /*
    * Indirect Command buffer
    */

    std::vector<GPUBatch> commandBufferData(_objectsBatches.size(), GPUBatch{});
    size_t offset = 0;
    for (int i = 0; i < _objectsBatches.size(); ++i) {
        GPUBatch& gpuBatch = commandBufferData[i];
        gpuBatch.command.firstInstance = offset;  // Used to access i in the model matrix since we dont use instancing.
        gpuBatch.command.instanceCount = 0;
        gpuBatch.command.indexCount = _objectsBatches[i].primitivesPerObject;
        _nbInstances += _objectsBatches[i].nbObjects;
        offset += _objectsBatches[i].nbObjects;
    }
    //_createGPUBuffer(commandBufferData.size() * sizeof(GPUBatch),
    //    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
    //    commandBufferData.data(),
    //    _gpuBatches
    //);

    VkDeviceSize commandBufferSize = _objectsBatches.size() * sizeof(GPUBatch);
    _createBuffer(commandBufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        _gpuBatches);
    {
        void* voidDataPtr = nullptr;
        GPUBatch* commandBufferPtr = nullptr;
        vkMapMemory(_device, _gpuBatches.deviceMemory, 0, commandBufferSize, 0, &voidDataPtr);
        commandBufferPtr = static_cast<GPUBatch*>(voidDataPtr);
        offset = 0;
        for (int i = 0; i < _objectsBatches.size(); ++i) {
            size_t stride = _objectsBatches[i].nbObjects;
            GPUBatch& gpuBatch = commandBufferPtr[i];
            gpuBatch.command.firstInstance = offset;  // Used to access i in the model matrix since we dont use instancing.
            gpuBatch.command.instanceCount = 0;
            gpuBatch.command.indexCount = _objectsBatches[i].primitivesPerObject;
            gpuBatch.command.firstIndex = 0;
            gpuBatch.command.vertexOffset = 0;
            offset += stride;
        }
        vkUnmapMemory(_device, _gpuBatches.deviceMemory);
    }
    _testBatchesSize = _objectsBatches.size();

    void* voidDataPtr = nullptr;
    GPUBatch* commandBufferPtr = nullptr;
    vkMapMemory(_device, _gpuBatches.deviceMemory, 0, _testBatchesSize * sizeof(GPUBatch), 0, &voidDataPtr);
    commandBufferPtr = static_cast<GPUBatch*>(voidDataPtr);
    for (int i = 0; i < _testBatchesSize; i++) {
        GPUBatch& batch = commandBufferPtr[i];
        if (batch.command.instanceCount == 0) {
            //std::cout << i << std::endl;
        }
        int a = 0;
    }
    vkUnmapMemory(_device, _gpuBatches.deviceMemory);

    /*
    * Indirect Command buffer reset
    */

    _createGPUBuffer(commandBufferData.size() * sizeof(GPUBatch),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        commandBufferData.data(),
        _gpuResetBatches
    );

    /*
    * Instances buffer
    */

    uint32_t nbObjects = static_cast<size_t>(scene->objects.size());

    std::vector<GPUObjectEntry> objects(nbObjects);
    {
        int entryIdx = 0;
        for (int batchIdx = 0; batchIdx < _objectsBatches.size(); ++batchIdx) {
            for (int i = 0; i < _objectsBatches[batchIdx].nbObjects; ++i) {
                objects[entryIdx].batchId = batchIdx;
                objects[entryIdx].dataId = entryIdx;
                entryIdx++;
            }
        }
    }
    _createGPUBuffer(nbObjects * sizeof(GPUObjectEntry),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        objects.data(),
        _gpuObjectEntries
    );

    _createGPUBuffer(nbObjects * sizeof(uint32_t),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        objects.data(),
        _gpuIndexToObjectId
    );

    GPUCullingGlobalData globalData;
    glm::mat4 projectionT = glm::transpose(_projectionMatrix);
    globalData.frustum[0] = projectionT[3] + projectionT[0];
    globalData.frustum[1] = projectionT[3] - projectionT[0];
    globalData.frustum[2] = projectionT[3] + projectionT[1];
    globalData.frustum[3] = projectionT[3] - projectionT[1];
    globalData.zNear = _zNear;
    globalData.zFar = _zFar;
    globalData.P00 = projectionT[0][0];
    globalData.P11 = projectionT[1][1];
    globalData.pyramidWidth = _depthPyramidWidth;
    globalData.pyramidHeight = _depthPyramidHeight;
    globalData.nbInstances = nbObjects;
    leo::Camera camera;
    camera.setPosition(glm::vec3(0, 0, 0));
    camera.setFront(glm::vec3(0, 0, 1));
    globalData.viewMatrix = glm::lookAt(camera.getPosition(), camera.getPosition() + camera.getFront(), camera.getUp());
    _createGPUBuffer(sizeof(GPUCullingGlobalData),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        &globalData,
        _gpuCullingGlobalData
    );

    /*
    * Setup descriptors
    */

    _createGlobalDescriptors(nbObjects);
    _createCullingDescriptors(nbObjects);

    /*
    * Culling data barriers
    */

    _gpuIndexToObjectIdBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    _gpuIndexToObjectIdBarrier.pNext = nullptr;
    _gpuIndexToObjectIdBarrier.buffer = _gpuIndexToObjectId.buffer;
    _gpuIndexToObjectIdBarrier.size = VK_WHOLE_SIZE;
    _gpuIndexToObjectIdBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    _gpuIndexToObjectIdBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    _gpuIndexToObjectIdBarrier.srcQueueFamilyIndex = static_cast<uint32_t>(_vulkan->getQueueFamilyIndices().graphicsFamily.value());

    _gpuBatchesBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    _gpuBatchesBarrier.pNext = nullptr;
    _gpuBatchesBarrier.buffer = _gpuBatches.buffer;
    _gpuBatchesBarrier.size = VK_WHOLE_SIZE;
    _gpuBatchesBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    _gpuBatchesBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    _gpuBatchesBarrier.srcQueueFamilyIndex = static_cast<uint32_t>(_vulkan->getQueueFamilyIndices().graphicsFamily.value());

    _gpuBatchesResetBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    _gpuBatchesResetBarrier.pNext = nullptr;
    _gpuBatchesResetBarrier.buffer = _gpuBatches.buffer;
    _gpuBatchesResetBarrier.size = VK_WHOLE_SIZE;
    _gpuBatchesResetBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    _gpuBatchesResetBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
    _gpuBatchesResetBarrier.srcQueueFamilyIndex = static_cast<uint32_t>(_vulkan->getQueueFamilyIndices().graphicsFamily.value());
}

void VulkanRenderer::_createRenderPass()
{
    const VulkanInstance::Properties& instanceProperties = _vulkan->getProperties();

    VkAttachmentDescription2 colorAttachment = {};
    colorAttachment.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
    colorAttachment.format = instanceProperties.swapChainImageFormat;
    colorAttachment.samples = instanceProperties.maxNbMsaaSamples;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    //colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;  // ONE_SAMPLE
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;  // ONE_SAMPLE
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    //colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;  // ONE_SAMPLE
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;  // ONE_SAmple

    VkAttachmentReference2 colorAttachmentRef = {};
    colorAttachmentRef.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachmentRef.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    VkAttachmentDescription2 depthAttachment = {};
    depthAttachment.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
    depthAttachment.format = _depthBufferFormat;
    depthAttachment.samples = instanceProperties.maxNbMsaaSamples;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    //depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;  // ONE_SAMPLE
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    //depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;  // ONE_SAMPLE
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;  // ONE_SAMPLE

    VkAttachmentReference2 depthAttachmentRef = {};
    depthAttachmentRef.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachmentRef.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    /*
    VkAttachmentDescription2 colorAttachmentResolve = {};
    colorAttachmentResolve.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
    colorAttachmentResolve.format = instanceProperties.swapChainImageFormat;
    colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference2 colorAttachmentResolveRef = {};
    colorAttachmentResolveRef.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
    colorAttachmentResolveRef.attachment = 2;
    colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachmentResolveRef.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    VkAttachmentDescription2 depthAttachmentResolve = {};
    depthAttachmentResolve.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
    depthAttachmentResolve.format = _depthBufferFormat;
    depthAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference2 depthAttachmentResolveRef = {};
    depthAttachmentResolveRef.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
    depthAttachmentResolveRef.attachment = 3;
    depthAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachmentResolveRef.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

    VkSubpassDescriptionDepthStencilResolve subpassDepthSencilResolve = {};
    subpassDepthSencilResolve.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE;
    subpassDepthSencilResolve.stencilResolveMode = VK_RESOLVE_MODE_NONE;
    subpassDepthSencilResolve.depthResolveMode = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
    subpassDepthSencilResolve.pDepthStencilResolveAttachment = &depthAttachmentResolveRef;
    */  // ONE_SAMPLE

    VkSubpassDescription2 subpass = {};
    subpass.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2;
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;
    //subpass.pResolveAttachments = &colorAttachmentResolveRef;  // ONE_SAMPLE
    //subpass.pNext = &subpassDepthSencilResolve;  // ONE_SAMPLE
    subpass.pResolveAttachments = nullptr;  // ONE_SAMPLE
    subpass.pNext = nullptr;  // ONE_SAMPLE

    VkSubpassDependency2 dependency = {};
    dependency.sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    /*std::array<VkAttachmentDescription2, 2> attachments = {
        colorAttachment,
        depthAttachment,
        colorAttachmentResolve,
        depthAttachmentResolve
    };*/  // ONE_SAMPLE

    std::array<VkAttachmentDescription2, 2> attachments = {
        colorAttachment,
        depthAttachment
    };

    VkRenderPassCreateInfo2 renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VK_CHECK(vkCreateRenderPass2(_device, &renderPassInfo, nullptr, &_renderPass));
}

void VulkanRenderer::_createFramebuffersImage()
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
        _framebufferColor);

    _vulkan->createImageView(_framebufferColor.image, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1, _framebufferColor.view);

    _vulkan->createImage(
        instanceProperties.swapChainExtent.width,
        instanceProperties.swapChainExtent.height,
        1,
        instanceProperties.maxNbMsaaSamples,
        _depthBufferFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        _framebufferDepth);

    _vulkan->createImageView(_framebufferDepth.image, _depthBufferFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1, _framebufferDepth.view);
}

void VulkanRenderer::_createGlobalDescriptors(uint32_t nbObjects)
{
    DescriptorAllocator::Options globalDescriptorAllocatorOptions = {};
    globalDescriptorAllocatorOptions.poolBaseSize = 10;
    globalDescriptorAllocatorOptions.poolSizes = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1.f },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1.f },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2.f },
    };
    _globalDescriptorAllocator.init(globalDescriptorAllocatorOptions);

    DescriptorBuilder builder = DescriptorBuilder::begin(_device, _globalDescriptorLayoutCache, _globalDescriptorAllocator);

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

    VkDescriptorBufferInfo indexMapInfo = {};
    indexMapInfo.buffer = _gpuIndexToObjectId.buffer;
    indexMapInfo.offset = 0;
    indexMapInfo.range = VK_WHOLE_SIZE;

    DescriptorBuilder::begin(_device, _globalDescriptorLayoutCache, _globalDescriptorAllocator)
        .bindBuffer(0, cameraBufferInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT)
        .bindBuffer(1, sceneBufferInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
        .bindBuffer(2, indexMapInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
        .build(_globalDataDescriptorSet, _globalDataDescriptorSetLayout);

    /*
    * Per-object data (set 1)
    */

    VkDescriptorBufferInfo objectsDataBufferInfo = {};
    objectsDataBufferInfo.buffer = _objectsDataBuffer.buffer;
    objectsDataBufferInfo.offset = 0;
    objectsDataBufferInfo.range = nbObjects * sizeof(GPUObjectData);

    DescriptorBuilder::begin(_device, _globalDescriptorLayoutCache, _globalDescriptorAllocator)
        .bindBuffer(0, objectsDataBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
        .build(_objectsDataDescriptorSet, _objectsDataDescriptorSetLayout);
}

void VulkanRenderer::_createFramebuffers()
{
    const std::vector<VkImageView>& swapChainImageViews = _vulkan->getSwapChainImageViews();

    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        /*std::array<VkImageView, 4> attachments = {
            _framebufferColor.view,
            _framebufferDepth.view,
            swapChainImageViews[i],
            _depthImage.view
        };*/  // ONE_SAMPLE
        std::array<VkImageView, 2> attachments = {
            swapChainImageViews[i],
            _depthImage.view
        };  // ONE_SAMPLE

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

void VulkanRenderer::_createCullingDescriptors(uint32_t nbObjects)
{
    DescriptorAllocator::Options cullingDescriptorAllocatorOptions = {};
    cullingDescriptorAllocatorOptions.poolBaseSize = 10;
    cullingDescriptorAllocatorOptions.poolSizes = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1.f },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1.f },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4.f },
    };
    _cullingDescriptorAllocator.init(cullingDescriptorAllocatorOptions);

    VkDescriptorBufferInfo globalDataBufferInfo = {};
    globalDataBufferInfo.buffer = _gpuCullingGlobalData.buffer;
    globalDataBufferInfo.offset = 0;
    globalDataBufferInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo cameraBufferInfo = {};
    cameraBufferInfo.buffer = _cameraDataBuffer.buffer;
    cameraBufferInfo.offset = 0;
    cameraBufferInfo.range = sizeof(GPUCameraData);

    VkDescriptorBufferInfo objectsDataBufferInfo = {};
    objectsDataBufferInfo.buffer = _objectsDataBuffer.buffer;
    objectsDataBufferInfo.offset = 0;
    objectsDataBufferInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo drawBufferInfo = {};
    drawBufferInfo.buffer = _gpuBatches.buffer;
    drawBufferInfo.offset = 0;
    drawBufferInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo instancesInfo = {};
    instancesInfo.buffer = _gpuObjectEntries.buffer;
    instancesInfo.offset = 0;
    instancesInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo indexMapInfo = {};
    indexMapInfo.buffer = _gpuIndexToObjectId.buffer;
    indexMapInfo.offset = 0;
    indexMapInfo.range = VK_WHOLE_SIZE;

    VkDescriptorImageInfo depthPyramidInfo = {};
    depthPyramidInfo.sampler = _depthImage.textureSampler;
    depthPyramidInfo.imageView = _depthPyramid.view;
    depthPyramidInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    DescriptorBuilder::begin(_device, _globalDescriptorLayoutCache, _cullingDescriptorAllocator)
        .bindBuffer(0, globalDataBufferInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
        .bindBuffer(1, cameraBufferInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_COMPUTE_BIT)
        .bindBuffer(2, objectsDataBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
        .bindBuffer(3, drawBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
        .bindBuffer(4, instancesInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
        .bindBuffer(5, indexMapInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
        .bindImage(6, depthPyramidInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
        .build(_cullingDescriptorSet, _cullingDescriptorSetLayout);
}

void VulkanRenderer::_createDepthPyramidDescriptors()
{
    _depthPyramidDescriptorSets.resize(_depthPyramid.mipLevels, VK_NULL_HANDLE);

    DescriptorAllocator::Options depthPyramidDescriptorAllocatorOptions = {};
    depthPyramidDescriptorAllocatorOptions.poolBaseSize = _depthPyramid.mipLevels;
    depthPyramidDescriptorAllocatorOptions.poolSizes = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1.f },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.f },
    };
    _depthPyramidDescriptorAllocator.init(depthPyramidDescriptorAllocatorOptions);

    for (int i = 0; i < _depthPyramid.mipLevels; ++i) {
        VkDescriptorImageInfo srcInfo = {};
        if (i == 0) {
            srcInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            srcInfo.imageView = _depthImage.view;
        }
        else {
            srcInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            srcInfo.imageView = _depthPyramidLevelViews[i - 1];
        }
        srcInfo.sampler = _depthImage.textureSampler;

        VkDescriptorImageInfo dstInfo;
        dstInfo.sampler = _depthImage.textureSampler;
        dstInfo.imageView = _depthPyramidLevelViews[i];
        dstInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        DescriptorBuilder::begin(_device, _globalDescriptorLayoutCache, _depthPyramidDescriptorAllocator)
            .bindImage(0, dstInfo, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
            .bindImage(1, srcInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(_depthPyramidDescriptorSets[i], _depthPyramidDescriptorSetLayout);
    }
}

void VulkanRenderer::_createOcclusionCullingData()
{
    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.minLod = 0;
    samplerCreateInfo.maxLod = 16.f;
    VkSamplerReductionModeCreateInfo reductionCreateInfo = {};
    reductionCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO;
    reductionCreateInfo.reductionMode = VK_SAMPLER_REDUCTION_MODE_MIN;
    samplerCreateInfo.pNext = &reductionCreateInfo;

    VK_CHECK(vkCreateSampler(_device, &samplerCreateInfo, 0, &_depthImage.textureSampler));

    const VulkanInstance::Properties& instanceProperties = _vulkan->getProperties();
    _depthPyramidWidth = previousPow2(instanceProperties.swapChainExtent.width);
    _depthPyramidHeight = previousPow2(instanceProperties.swapChainExtent.height);
    _depthPyramid.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(instanceProperties.swapChainExtent.width, instanceProperties.swapChainExtent.height)))) + 1;

    VkExtent3D depthPyramidExtent = {
        static_cast<uint32_t>(_depthPyramidWidth),
        static_cast<uint32_t>(_depthPyramidHeight),
        1
    };

    _vulkan->createImage(_depthPyramidWidth, _depthPyramidHeight, _depthPyramid.mipLevels,
        VK_SAMPLE_COUNT_1_BIT,
        VK_FORMAT_R32_SFLOAT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        _depthPyramid);

    _transitionImageLayout(_depthPyramid, VK_FORMAT_R32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    _vulkan->createImageView(_depthPyramid.image, VK_FORMAT_R32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, _depthPyramid.mipLevels, _depthPyramid.view);

    _depthPyramidLevelViews.resize(_depthPyramid.mipLevels, VK_NULL_HANDLE);
    for (int i = 0; i < _depthPyramid.mipLevels; ++i) {
        _vulkan->createImageView(_depthPyramid.image, VK_FORMAT_R32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 1, _depthPyramidLevelViews[i], i);
    }

    _depthPyramidMipLevelBarriers.resize(_depthPyramid.mipLevels, {});
    for (int i = 0; i < _depthPyramid.mipLevels; ++i) {
        VkImageMemoryBarrier& barrier = _depthPyramidMipLevelBarriers[i];
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = _depthPyramid.image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = i;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
    }

    _framebufferDepthWriteBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    _framebufferDepthWriteBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    _framebufferDepthWriteBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    _framebufferDepthWriteBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    _framebufferDepthWriteBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    _framebufferDepthWriteBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    _framebufferDepthWriteBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    _framebufferDepthWriteBarrier.image = _depthImage.image;
    _framebufferDepthWriteBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    _framebufferDepthWriteBarrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    _framebufferDepthWriteBarrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    _framebufferDepthReadBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    _framebufferDepthReadBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    _framebufferDepthReadBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    _framebufferDepthReadBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    _framebufferDepthReadBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    _framebufferDepthReadBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    _framebufferDepthReadBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    _framebufferDepthReadBarrier.image = _depthImage.image;
    _framebufferDepthReadBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    _framebufferDepthReadBarrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    _framebufferDepthReadBarrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

}

void VulkanRenderer::_testWriteDepthBufferToDisc() {
    vkQueueWaitIdle(_vulkan->getGraphicsQueue());
    uint32_t width = _vulkan->getProperties().swapChainExtent.width;
    uint32_t height = _vulkan->getProperties().swapChainExtent.height;

    VkDeviceSize size = width * height * 4;

    AllocatedBuffer copyBuffer;
    _createBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, copyBuffer);

    _transitionImageLayout(_depthImage, _depthBufferFormat, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);

    VkCommandBuffer cmd = _beginSingleTimeCommands(_mainCommandPool);

    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { width, height, 1 };

    vkCmdCopyImageToBuffer(cmd, _depthImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, copyBuffer.buffer, 1, &region);

    _endSingleTimeCommands(cmd, _mainCommandPool);
    
    void* data = nullptr;
    vkMapMemory(_device, copyBuffer.deviceMemory, 0, size, 0, &data);
    float* dataPtr = static_cast<float*>(data);

    std::ofstream file("bleubleu.ppm", std::ios::out | std::ios::binary);

    // ppm header
    file << "P3\n" << width << "\n" << height << "\n" << 255 << "\n";

    for (int i = 0; i < width * height; ++i) {
        float val = dataPtr[i];
        //val = val * 2.0f - 1.0f;
        //val = (2.0f * _zNear * _zFar) / (_zFar + _zNear - val * (_zFar - _zNear));
        //val = (2.0 * _zNear) / (_zFar + _zNear - val * (_zFar - _zNear));
        //val = _zNear * _zFar / (_zFar + val * (_zNear - _zFar));
        file << int(val * 255.f) << " " << int(val * 255.f) << " " << int(val * 255.f) << std::endl;
    }
    file.flush();

    file.close();
    vkUnmapMemory(_device, copyBuffer.deviceMemory);

    vkDestroyBuffer(_device, copyBuffer.buffer, nullptr);
    vkFreeMemory(_device, copyBuffer.deviceMemory, nullptr);
}

void VulkanRenderer::_testWriteDepthPyramidToDisc() {
    vkQueueWaitIdle(_vulkan->getGraphicsQueue());
    int mip = 4;
    uint32_t width = _depthPyramidWidth / pow(2, mip);
    uint32_t height = _depthPyramidHeight / pow(2, mip);

    VkDeviceSize size = width * height * 4;

    AllocatedBuffer copyBuffer;
    _createBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, copyBuffer);

    _transitionImageLayout(_depthPyramid, VK_FORMAT_R32_SFLOAT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

    VkCommandBuffer cmd = _beginSingleTimeCommands(_mainCommandPool);

    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = mip;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { width, height, 1 };

    vkCmdCopyImageToBuffer(cmd, _depthPyramid.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, copyBuffer.buffer, 1, &region);

    _endSingleTimeCommands(cmd, _mainCommandPool);

    void* data = nullptr;
    vkMapMemory(_device, copyBuffer.deviceMemory, 0, size, 0, &data);
    float* dataPtr = static_cast<float*>(data);

    std::ofstream file("pleupleu.ppm", std::ios::out | std::ios::binary);

    // ppm header
    file << "P3\n" << width << "\n" << height << "\n" << 255 << "\n";

    for (int i = 0; i < width * height; ++i) {
        float val = dataPtr[i];
        file << int(val * 255.f) << " " << int(val * 255.f) << " " << int(val * 255.f) << std::endl;
    }
    file.flush();

    file.close();
    vkUnmapMemory(_device, copyBuffer.deviceMemory);

    vkDestroyBuffer(_device, copyBuffer.buffer, nullptr);
    vkFreeMemory(_device, copyBuffer.deviceMemory, nullptr);
}

/*
void VulkanRenderer::_testWriteDepthBufferToDisc() {
    vkQueueWaitIdle(_vulkan->getGraphicsQueue());
    int mip = 0;
    uint32_t width = _depthPyramidWidth / pow(2, mip);
    uint32_t height = _depthPyramidHeight / pow(2, mip);

    VkDeviceSize size = width * height * 4;

    AllocatedBuffer copyBuffer;
    _createBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, copyBuffer);

    _transitionImageLayout(_depthPyramid, VK_FORMAT_R32_SFLOAT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

    VkCommandBuffer cmd = _beginSingleTimeCommands(_mainCommandPool);

    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = mip;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { width, height, 1 };

    vkCmdCopyImageToBuffer(cmd, _depthPyramid.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, copyBuffer.buffer, 1, &region);

    _endSingleTimeCommands(cmd, _mainCommandPool);

    void* data = nullptr;
    vkMapMemory(_device, copyBuffer.deviceMemory, 0, size, 0, &data);
    float* dataPtr = static_cast<float*>(data);

    std::ofstream file("bleubleu.ppm", std::ios::out | std::ios::binary);

    // ppm header
    file << "P3\n" << width << "\n" << height << "\n" << 255 << "\n";

    for (int i = 0; i < width * height; ++i) {
        float val = dataPtr[i];
        file << int(val * 255.f) << " " << int(val * 255.f) << " " << int(val * 255.f) << std::endl;
    }
    file.flush();

    file.close();
    vkUnmapMemory(_device, copyBuffer.deviceMemory);

    vkDestroyBuffer(_device, copyBuffer.buffer, nullptr);
    vkFreeMemory(_device, copyBuffer.deviceMemory, nullptr);
}
*/

void VulkanRenderer::_createComputePipeline(const char* shaderPath, VkPipeline& pipeline, VkPipelineLayout& layout, ShaderPass& shaderPass)
{
    /*
    * Compute pipeline for culling
    */

    ShaderPass::Parameters shaderPassParameters = {};
    shaderPassParameters.device = _device;
    shaderPassParameters.shaderBuilder = &_shaderBuilder;
    shaderPassParameters.shaderPaths[VK_SHADER_STAGE_COMPUTE_BIT] = shaderPath;
    shaderPassParameters.descriptorTypeOverwrites["camera"] = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;

    layout = shaderPass.reflectShaderModules(shaderPassParameters);

    ComputePipelineBuilder computeBuilder;
    computeBuilder.pipelineLayout = layout;
    computeBuilder.shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computeBuilder.shaderStage.pNext = nullptr;
    computeBuilder.shaderStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computeBuilder.shaderStage.module = shaderPass.getShaderModules().at(VK_SHADER_STAGE_COMPUTE_BIT);
    computeBuilder.shaderStage.pName = "main";

    pipeline = computeBuilder.buildPipeline(_device);

    shaderPass.destroyShaderModules();
}

void VulkanRenderer::setCamera(const leo::Camera* camera)
{
    _camera = camera;

    const VkExtent2D& swapChainExtent = _vulkan->getProperties().swapChainExtent;
    _projectionMatrix = glm::perspective(glm::radians(45.0f), static_cast<float>(swapChainExtent.width) / swapChainExtent.height, _zNear, _zFar);
}

namespace {
    uint32_t previousPow2(uint32_t v) {
        uint32_t result = 1;
        while (result * 2 < v) result *= 2;
        return result;
    }
}
