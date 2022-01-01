#include "VulkanRenderer.h"

#include <scene/Scene.h>
#include <scene/PerformanceMaterial.h>
#include <scene/Mesh.h>
#include <scene/Transform.h>
#include <scene/Camera.h>
#include <scene/Camera.h>
#include <scene/GeometryIncludes.h>

#include "VulkanUtils.h"
#include "DebugUtils.h"

#include <iostream>
#include <array>
#include <fstream>
#include <set>


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

void VulkanRenderer::cleanup()
{
    vkDeviceWaitIdle(_device);

    /*
    * Cleanup descriptors and secondary data managers
    */

    _materialBuilder.cleanup();
    _cullingDescriptorAllocator.cleanup();
    _depthPyramidDescriptorAllocator.cleanup();
    _globalDescriptorAllocator.cleanup();
    _globalDescriptorLayoutCache.cleanup();
    _materialDescriptorSets.clear();
    _depthPyramidDescriptorSets.clear();
    _globalDataDescriptorSet = VK_NULL_HANDLE;
    _globalDataDescriptorSetLayout = VK_NULL_HANDLE;
    _objectsDataDescriptorSet = VK_NULL_HANDLE;
    _objectsDataDescriptorSetLayout = VK_NULL_HANDLE;
    _cullingDescriptorSet = VK_NULL_HANDLE;
    _cullingDescriptorSetLayout = VK_NULL_HANDLE;

    /*
    * Cleaning up scene data
    */

    if (_sceneLoaded) {
        // Culling and indirect draw buffers

        _vulkan->destroyBuffer(_gpuCullingGlobalData);
        _vulkan->destroyBuffer(_gpuIndexToObjectId);
        _vulkan->destroyBuffer(_gpuObjectInstances);
        _vulkan->destroyBuffer(_gpuResetBatches);
        _vulkan->destroyBuffer(_gpuBatches);

        // Scene objects data

        _vulkan->destroyBuffer(_objectsDataBuffer);

        for (const std::unique_ptr<ShapeData>& shapeData : _shapeData) {
            _vulkan->destroyBuffer(shapeData->indexBuffer);
            _vulkan->destroyBuffer(shapeData->vertexBuffer);
        }
        _shapeData.clear();

        for (VkSampler materialImageSampler : _materialImagesSamplers) {
            vkDestroySampler(_device, materialImageSampler, nullptr);
        }
        _materialImagesSamplers.clear();

        for (const std::unique_ptr<AllocatedImage>& materialImage : _materialImagesData) {
            vkDestroyImageView(_device, materialImage->view, nullptr);
            _vulkan->destroyImage(*materialImage);
        }
        _materialImagesData.clear();
    }

    /*
    * Cleanup initial renderer data
    */

    // Depth pyramid

    for (VkImageView view : _depthPyramidLevelViews) {
        vkDestroyImageView(_device, view, nullptr);
    }
    _depthPyramidLevelViews.clear();

    vkDestroyImageView(_device, _depthPyramid.view, nullptr);
    _vulkan->destroyImage(_depthPyramid);

    vkDestroySampler(_device, _depthImageSampler, nullptr);
    _depthImageSampler = VK_NULL_HANDLE;

    // Global data for shaders

    _vulkan->destroyBuffer(_sceneDataBuffer);
    _vulkan->destroyBuffer(_cameraDataBuffer);

    // Culling pipelines and passes

    _cullShaderPass.cleanup();
    vkDestroyPipeline(_device, _cullingPipeline, nullptr);
    _cullingPipeline = VK_NULL_HANDLE;
    vkDestroyPipelineLayout(_device, _cullingPipelineLayout, nullptr);
    _cullingPipelineLayout = VK_NULL_HANDLE;

    _depthPyramidShaderPass.cleanup();
    vkDestroyPipeline(_device, _depthPyramidPipeline, nullptr);
    _depthPyramidPipeline = VK_NULL_HANDLE;
    vkDestroyPipelineLayout(_device, _depthPyramidPipelineLayout, nullptr);
    _depthPyramidPipelineLayout = VK_NULL_HANDLE;

    /*
    * Frames data
    */

    for (FrameData& frameData : _framesData) {
        vkDestroySemaphore(_device, frameData.presentSemaphore, nullptr);
        vkDestroySemaphore(_device, frameData.renderSemaphore, nullptr);
        vkDestroyFence(_device, frameData.renderFinishedFence, nullptr);
        vkDestroyFramebuffer(_device, frameData.framebuffer, nullptr);
    }

    /*
    * Framebuffer attachments
    */

    vkDestroyImageView(_device, _depthImage.view, nullptr);
    _vulkan->destroyImage(_depthImage);

    vkDestroyImageView(_device, _framebufferDepth.view, nullptr);
    _vulkan->destroyImage(_framebufferDepth);

    vkDestroyImageView(_device, _framebufferColor.view, nullptr);
    _vulkan->destroyImage(_framebufferColor);

    /*
    * Render pass
    */

    vkDestroyRenderPass(_device, _renderPass, nullptr);
    _renderPass = VK_NULL_HANDLE;

    /*
    * Command pools
    */

    for (FrameData& frameData : _framesData) {
        vkDestroyCommandPool(_device, frameData.commandPool, nullptr);
    }
    _framesData.clear();

    vkDestroyCommandPool(_device, _mainCommandPool, nullptr);
    _mainCommandPool = VK_NULL_HANDLE;
}

void VulkanRenderer::cleanupSwapChainDependentObjects()
{
    vkDeviceWaitIdle(_device);

    /*
    * Image resources depending on framebuffer's dimensions
    */

    // Framebuffers' attachments

    vkDestroyImageView(_device, _depthImage.view, nullptr);
    _vulkan->destroyImage(_depthImage);

    vkDestroyImageView(_device, _framebufferDepth.view, nullptr);
    _vulkan->destroyImage(_framebufferDepth);

    vkDestroyImageView(_device, _framebufferColor.view, nullptr);
    _vulkan->destroyImage(_framebufferColor);

    // Depth pyramid

    for (VkImageView view : _depthPyramidLevelViews) {
        vkDestroyImageView(_device, view, nullptr);
    }
    _depthPyramidLevelViews.clear();

    vkDestroyImageView(_device, _depthPyramid.view, nullptr);
    _vulkan->destroyImage(_depthPyramid);

    vkDestroySampler(_device, _depthImageSampler, nullptr);
    _depthImageSampler = VK_NULL_HANDLE;

    /*
    * Framebuffers
    */

    for (FrameData& frameData : _framesData) {
        vkDestroyFramebuffer(_device, frameData.framebuffer, nullptr);
    }

    /*
    * Command buffers
    */

    for (FrameData& frameData : _framesData) {
        vkFreeCommandBuffers(_device, frameData.commandPool, 1, &frameData.commandBuffer);
    }

    /*
    * Compute pipelines using screen size dependent resources
    */

    _cullShaderPass.cleanup();
    vkDestroyPipeline(_device, _cullingPipeline, nullptr);
    _cullingPipeline = VK_NULL_HANDLE;
    vkDestroyPipelineLayout(_device, _cullingPipelineLayout, nullptr);
    _cullingPipelineLayout = VK_NULL_HANDLE;

    _depthPyramidShaderPass.cleanup();
    vkDestroyPipeline(_device, _depthPyramidPipeline, nullptr);
    _depthPyramidPipeline = VK_NULL_HANDLE;
    vkDestroyPipelineLayout(_device, _depthPyramidPipelineLayout, nullptr);
    _depthPyramidPipelineLayout = VK_NULL_HANDLE;

    /*
    * Descriptors of screen size dependent resources
    */
    _cullingDescriptorAllocator.cleanup();
    _depthPyramidDescriptorAllocator.cleanup();
    _depthPyramidDescriptorSets.clear();

    /*
    * Render pass
    */

    vkDestroyRenderPass(_device, _renderPass, nullptr);
    _renderPass = VK_NULL_HANDLE;

    _vulkan->cleanupSwapChain();
}

void VulkanRenderer::recreateSwapChainDependentObjects()
{
    _vulkan->recreateSwapChain();

    _createMainRenderPass();

    _createFramebuffers();

    for (FrameData& frame : _framesData) {
        VkCommandBufferAllocateInfo allocInfo = VulkanUtils::createCommandBufferAllocateInfo(frame.commandPool, 1);
        VK_CHECK(vkAllocateCommandBuffers(_device, &allocInfo, &frame.commandBuffer));
    }

    _createComputePipeline("resources/shaders/depth_pyramid.spv", _depthPyramidPipeline, _depthPyramidPipelineLayout, _depthPyramidShaderPass);
    _createComputePipeline("resources/shaders/indirect_cull.spv", _cullingPipeline, _cullingPipelineLayout, _cullShaderPass);

    _createDepthSampler();
    _createDepthPyramid();

    _createCullingDescriptors(_totalInstancesNb);
    _createDepthPyramidDescriptors();

    _createBarriers();
}

void VulkanRenderer::init()
{
    const VulkanInstance::QueueFamilyIndices& queueFamilyIndices = _vulkan->getQueueFamilyIndices();
    size_t nbSwapChainImages = _vulkan->getSwapChainImageViews().size();
    const VulkanInstance::Properties& instanceProperties = _vulkan->getProperties();
    const std::vector<VkImageView>& swapChainImageViews = _vulkan->getSwapChainImageViews();
    VkExtent2D swapChainExtent = _vulkan->getProperties().swapChainExtent;

    _depthBufferFormat = _vulkan->findSupportedFormat(
        { VK_FORMAT_D32_SFLOAT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT
    );
    if (_depthBufferFormat == VK_FORMAT_UNDEFINED) {
        throw VulkanRendererException("Failed to find a supported format for the depth buffer.");
    }


    /*
    * Command Pools
    */

    VkCommandPoolCreateInfo poolInfo = VulkanUtils::createCommandPoolInfo(queueFamilyIndices.graphicsFamily.value(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    VK_CHECK(vkCreateCommandPool(_device, &poolInfo, nullptr, &_mainCommandPool));

    for (FrameData& frameData : _framesData) {
        VK_CHECK(vkCreateCommandPool(_device, &poolInfo, nullptr, &frameData.commandPool));
    }


    /*
    * Create main render pass
    */

    _createMainRenderPass();

    /*
    * Framebuffers and their attachments
    */

    _createFramebuffers();

    /*
    * Command buffers for each frame
    */

    for (FrameData& frame : _framesData) {
        VkCommandBufferAllocateInfo allocInfo = VulkanUtils::createCommandBufferAllocateInfo(frame.commandPool, 1);
        VK_CHECK(vkAllocateCommandBuffers(_device, &allocInfo, &frame.commandBuffer));
    }


    /*
    * Sync objects for rendering to each frame
    */

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


    /*
    * Create pipelines
    */

    // Graphics pipelines for each material type (one!)
    _materialBuilder.init({ instanceProperties.maxNbMsaaSamples, _renderPass });

    // Depth pyramid creation from depth buffer. Required for occlusion culling.
    _createComputePipeline("resources/shaders/depth_pyramid.spv", _depthPyramidPipeline, _depthPyramidPipelineLayout, _depthPyramidShaderPass);

    // Compute pipeline for culling (occlusion and frustum culling)
    _createComputePipeline("resources/shaders/indirect_cull.spv", _cullingPipeline, _cullingPipelineLayout, _cullShaderPass);


    /*
    * Global, non scene-related buffers
    */

    // Camera dynamic buffer
    uint32_t minAlignment = static_cast<uint32_t>(_vulkan->padUniformBufferSize(sizeof(GPUCameraData)));
    VkDeviceSize cameraBufferSize = nbSwapChainImages * minAlignment;
    _vulkan->createBuffer(cameraBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, _cameraDataBuffer, minAlignment);


    // Global scene data buffer
    size_t sceneDataBufferSize = sizeof(GPUSceneData);
    _vulkan->createBuffer(sceneDataBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU, _sceneDataBuffer);

    // Depth Sampler
    _createDepthSampler();

    // Depth pyramid for occlusion culling
    _createDepthPyramid();

    /*
    * Descriptors for depth-pyramid compute shader
    */

    _createDepthPyramidDescriptors();

    /*
    * Barriers
    */

    _createBarriers();
}

void VulkanRenderer::_createMainRenderPass()
{
    const VulkanInstance::Properties& instanceProperties = _vulkan->getProperties();

    VkAttachmentDescription2 colorAttachment = {};
    colorAttachment.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
    colorAttachment.format = instanceProperties.swapChainImageFormat;
    colorAttachment.samples = instanceProperties.maxNbMsaaSamples;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

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
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference2 depthAttachmentRef = {};
    depthAttachmentRef.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachmentRef.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

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

    VkSubpassDescription2 subpass = {};
    subpass.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2;
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;
    subpass.pResolveAttachments = &colorAttachmentResolveRef;
    subpass.pNext = &subpassDepthSencilResolve;

    VkSubpassDependency2 dependency = {};
    dependency.sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription2, 4> attachments = {
        colorAttachment,
        depthAttachment,
        colorAttachmentResolve,
        depthAttachmentResolve
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
        cleanupSwapChainDependentObjects();
        recreateSwapChainDependentObjects();
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
    indirectCopy.size = static_cast<uint32_t>(_drawCalls.size() * sizeof(GPUIndirectDrawCommand));
    indirectCopy.srcOffset = 0;
    vkCmdCopyBuffer(_framesData[imageIndex].commandBuffer, _gpuResetBatches.buffer, _gpuBatches.buffer, 1, &indirectCopy);

    vkCmdPipelineBarrier(_framesData[imageIndex].commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &_gpuBatchesResetBarrier, 0, nullptr);

    uint32_t uniformOffset = static_cast<uint32_t>(_vulkan->padUniformBufferSize(sizeof(GPUCameraData)) * _currentFrame);
    vkCmdBindDescriptorSets(_framesData[imageIndex].commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
        _cullingPipelineLayout, 0, 1, &_cullingDescriptorSet, 1, &uniformOffset);

    uint32_t groupCountX = static_cast<uint32_t>((_nbInstances / 256) + 1);
    vkCmdDispatch(_framesData[imageIndex].commandBuffer, groupCountX, 1, 1);

    std::array<VkBufferMemoryBarrier, 2> barriers = { _gpuIndexToObjectIdBarrier, _gpuBatchesBarrier };

    vkCmdPipelineBarrier(_framesData[imageIndex].commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
        0, 0, nullptr, static_cast<uint32_t>(barriers.size()), barriers.data(), 0, nullptr);

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

    const VkExtent2D& swapChainExtent = _vulkan->getProperties().swapChainExtent;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapChainExtent.width);
    viewport.height = static_cast<float>(swapChainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = swapChainExtent;

    vkCmdSetViewport(_framesData[imageIndex].commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(_framesData[imageIndex].commandBuffer, 0, 1, &scissor);

    // Global data descriptor set
    vkCmdBindDescriptorSets(_framesData[imageIndex].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        graphicsPipelineLayout, 0, 1, &_globalDataDescriptorSet, 1, &uniformOffset);

    uint32_t offset = 0;
    uint32_t stride = sizeof(GPUIndirectDrawCommand);
    for (const DrawCallInfo& batch : _drawCalls) {
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

    _computeDepthPyramid(_framesData[imageIndex].commandBuffer);

    VK_CHECK(vkEndCommandBuffer(_framesData[imageIndex].commandBuffer));

    VK_CHECK(vkQueueSubmit(_vulkan->getGraphicsQueue(), 1, &submitInfo, frameData.renderFinishedFence));

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
        cleanupSwapChainDependentObjects();
        recreateSwapChainDependentObjects();
    }

    _currentFrame = (_currentFrame + 1) % _MAX_FRAMES_IN_FLIGHT;
}

void VulkanRenderer::_computeDepthPyramid(VkCommandBuffer commandBuffer) {
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &_framebufferDepthWriteBarrier);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _depthPyramidPipeline);

    // TODO: Per-level barriers that would switch between read and write layout for the depth pyramid update.
    for (uint32_t i = 0; i < _depthPyramid.mipLevels; ++i)
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

void VulkanRenderer::_updateCamera(uint32_t currentImage) {
    /*
    * Camera data
    */

    GPUCameraData cameraData = {};
    glm::vec3 front = _camera->getFront();
    glm::vec3 up = _camera->getUp();
    glm::vec3 position = _camera->getPosition();
    position.y *= -1;

    cameraData.view = glm::lookAt(position, position + front, up);
    cameraData.proj = _projectionMatrix;
    cameraData.invProj = _invProjectionMatrix;
    cameraData.viewProj = cameraData.proj * cameraData.view;

    uint32_t cameraBufferPadding = static_cast<uint32_t>(_vulkan->padUniformBufferSize(sizeof(GPUCameraData)));
    _vulkan->copyDataToBuffer(static_cast<uint32_t>(sizeof(GPUCameraData)), _cameraDataBuffer, &cameraData, cameraBufferPadding * currentImage);
}


void VulkanRenderer::loadSceneToDevice(const leoscene::Scene* scene)
{
    /*
    * Loading scene objects to device
    */

    struct _ObjectInstanceData {
        const leoscene::Shape* shape = nullptr;
        const leoscene::Transform* transform = nullptr;
    };

    std::map<const leoscene::Material*, Material*> loadedMaterialsCache;
    std::map<const leoscene::ImageTexture*, AllocatedImage*> loadedImagesCache;
    std::map<const leoscene::ImageTexture*, VkSampler> loadedImageSamplersCache;
    std::map<const leoscene::Shape*, ShapeData*> shapeDataCache;
    std::map<const Material*, std::map<const ShapeData*, std::vector<_ObjectInstanceData>>> objectInstances;

    for (const leoscene::SceneObject& sceneObject : scene->objects) {
        const leoscene::PerformanceMaterial* sceneMaterial = static_cast<const leoscene::PerformanceMaterial*>(sceneObject.material.get());
        const leoscene::Shape* sceneShape = sceneObject.shape.get();
        Material* loadedMaterial = nullptr;
        ShapeData* loadedShape = nullptr;

        // Load material data on the device

        if (loadedMaterialsCache.find(sceneMaterial) == loadedMaterialsCache.end()) {
            loadedMaterial = _materialBuilder.createMaterial(MaterialType::BASIC);

            static const size_t nbTexturesInMaterial = 5;
            std::array<const leoscene::ImageTexture*, nbTexturesInMaterial> materialTextures = {
                sceneMaterial->diffuseTexture.get(), sceneMaterial->specularTexture.get(), sceneMaterial->ambientTexture.get(), sceneMaterial->normalsTexture.get(), sceneMaterial->heightTexture.get()
            };

            for (size_t i = 0; i < nbTexturesInMaterial; ++i) {
                const leoscene::ImageTexture* sceneTexture = materialTextures[i];
                AllocatedImage* loadedImage = nullptr;
                VkSampler loadedImageSampler = VK_NULL_HANDLE;
                if (loadedImagesCache.find(sceneTexture) == loadedImagesCache.end()) {
                    _materialImagesData.push_back(std::make_unique<AllocatedImage>());
                    loadedImage = _materialImagesData.back().get();

                    uint32_t texWidth = static_cast<uint32_t>(sceneTexture->width);
                    uint32_t texHeight = static_cast<uint32_t>(sceneTexture->height);

                    uint32_t imageMipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

                    uint32_t nbChannels = 0;
                    VkFormat imageFormat = VkFormat::VK_FORMAT_UNDEFINED;
                    switch (sceneTexture->layout) {
                    case leoscene::ImageTexture::Layout::R:
                        imageFormat = VK_FORMAT_R8_UNORM;
                        nbChannels = 1;
                        break;
                    case leoscene::ImageTexture::Layout::RGBA:
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

                    _vulkan->createImage(texWidth, texHeight, imageMipLevels, VK_SAMPLE_COUNT_1_BIT, imageFormat, VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, *loadedImage);

                    VkCommandBuffer cmd = _vulkan->beginSingleTimeCommands(_mainCommandPool);
                    VkImageMemoryBarrier textureCopyDstBarrier = VulkanUtils::createImageBarrier(
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        loadedImage->image,
                        VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
                        0, loadedImage->mipLevels
                    );
                    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &textureCopyDstBarrier);
                    _vulkan->endSingleTimeCommands(cmd, _mainCommandPool);

                    _vulkan->copyDataToImage(_mainCommandPool, texWidth, texHeight, nbChannels, *loadedImage, sceneTexture->data);

                    _vulkan->generateMipmaps(_mainCommandPool, *loadedImage, imageFormat, texWidth, texHeight);

                    _vulkan->createImageView(loadedImage->image, imageFormat, VK_IMAGE_ASPECT_COLOR_BIT, loadedImage->mipLevels, loadedImage->view);

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

                    _materialImagesSamplers.emplace_back();
                    VK_CHECK(vkCreateSampler(_device, &samplerInfo, nullptr, &_materialImagesSamplers.back()));
                    loadedImageSampler = _materialImagesSamplers.back();

                    loadedImagesCache[sceneTexture] = loadedImage;
                    loadedImageSamplersCache[sceneTexture] = loadedImageSampler;
                }
                else {
                    loadedImage = loadedImagesCache[sceneTexture];
                    loadedImageSampler = loadedImageSamplersCache[sceneTexture];
                }

                loadedMaterial->textures[i].sampler = loadedImageSampler;
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

            const leoscene::Mesh* mesh = static_cast<const leoscene::Mesh*>(sceneShape);  // TODO: assuming the shape is a mesh for now

            // Vertex buffer
            _vulkan->createGPUBuffer(_mainCommandPool, sizeof(leoscene::Vertex) * mesh->vertices.size(),
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, mesh->vertices.data(),
                loadedShape->vertexBuffer);

            // Index buffer
            _vulkan->createGPUBuffer(_mainCommandPool, sizeof(mesh->indices[0]) * mesh->indices.size(),
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

        if (objectInstances.find(loadedMaterial) == objectInstances.end()) {
            objectInstances[loadedMaterial] = {};
        }
        if (objectInstances[loadedMaterial].find(loadedShape) == objectInstances[loadedMaterial].end()) {
            objectInstances[loadedMaterial][loadedShape] = {};
        }
        objectInstances[loadedMaterial][loadedShape].push_back({ sceneShape, sceneObject.transform.get() });
    }

    _nbMaterials = objectInstances.size();


    /*
    * Initializing the object batches used to compute draw indirect commands
    */

    for (const auto& materialShapesPair : objectInstances) {
        const Material* material = materialShapesPair.first;  // TODO: assuming material is Performance for now
        for (const auto& shapeNbPair : materialShapesPair.second) {
            const ShapeData* shape = shapeNbPair.first;  // TODO: assuming the shape is a mesh for now

            _drawCalls.push_back({ material, shape,
                static_cast<uint32_t>(shapeNbPair.second.size()),  // nbObjects
                shape->nbElements, // primitivesPerObject
                });
        }
    }


    /*
    * Filling global scene data
    */

    // TODO: Put actual values (maybe from options and/or leoscene::Scene)
    GPUSceneData sceneData;
    sceneData.ambientColor = { 1, 0, 0, 0 };
    sceneData.sunlightColor = { 0, 1, 0, 0 };
    sceneData.sunlightDirection = { 0, 0, 0, 1 };
    _vulkan->copyDataToBuffer(sizeof(GPUSceneData), _sceneDataBuffer, &sceneData);

    /*
    * Per-object data
    */

    size_t objectsDataBufferSize = scene->objects.size() * sizeof(GPUObjectData);

    std::vector<GPUObjectData> objectData(objectsDataBufferSize);
    int i = 0;
    for (const auto& materialShapesPair : objectInstances) {
        const Material* material = materialShapesPair.first;  // TODO: assuming material is Performance for now
        for (const auto& shapeNbPair : materialShapesPair.second) {
            const ShapeData* shape = shapeNbPair.first;  // TODO: assuming the shape is a mesh for now
            for (const _ObjectInstanceData& instanceData : shapeNbPair.second) {
                const glm::mat4& modelMatrix = instanceData.transform->getMatrix();
                objectData[i].modelMatrix = modelMatrix;

                // Computing sphere bounds of the object in world space
                const glm::vec4& sphereBounds = static_cast<const leoscene::Mesh*>(instanceData.shape)->boundingSphere;
                glm::vec4 transformedSphere = modelMatrix * glm::vec4(sphereBounds.x, sphereBounds.y, sphereBounds.z, 1);
                float maxScale = glm::max(glm::max(glm::length(modelMatrix[0]), glm::length(modelMatrix[1])), glm::length(modelMatrix[2]));
                transformedSphere.w = maxScale * sphereBounds.w;
                objectData[i].sphereBounds = transformedSphere;

                i++;
            }
        }
    }

    _vulkan->createGPUBuffer(_mainCommandPool,
        objectsDataBufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        objectData.data(),
        _objectsDataBuffer
    );


    /*
    * Indirect Command buffer
    */

    std::vector<GPUIndirectDrawCommand> commandBufferData(_drawCalls.size(), GPUIndirectDrawCommand{});
    uint32_t offset = 0;
    for (int i = 0; i < _drawCalls.size(); ++i) {
        GPUIndirectDrawCommand& gpuBatch = commandBufferData[i];
        gpuBatch.command.firstInstance = offset;  // Used to access i in the model matrix since we dont use instancing.
        gpuBatch.command.instanceCount = 0;
        gpuBatch.command.indexCount = _drawCalls[i].primitivesPerObject;
        _nbInstances += _drawCalls[i].nbObjects;
        offset += _drawCalls[i].nbObjects;
    }

    _vulkan->createGPUBuffer(_mainCommandPool, commandBufferData.size() * sizeof(GPUIndirectDrawCommand),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        commandBufferData.data(),
        _gpuBatches
    );


    /*
    * Indirect Command buffer reset
    */

    _vulkan->createGPUBuffer(_mainCommandPool, commandBufferData.size() * sizeof(GPUIndirectDrawCommand),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        commandBufferData.data(),
        _gpuResetBatches
    );


    /*
    * Instances buffer
    */

    _totalInstancesNb = static_cast<uint32_t>(scene->objects.size());

    std::vector<GPUObjectInstance> objects(_totalInstancesNb);
    {
        uint32_t entryIdx = 0;
        for (uint32_t batchIdx = 0; batchIdx < _drawCalls.size(); ++batchIdx) {
            for (uint32_t i = 0; i < _drawCalls[batchIdx].nbObjects; ++i) {
                objects[entryIdx].batchId = batchIdx;
                objects[entryIdx].dataId = entryIdx;
                entryIdx++;
            }
        }
    }
    _vulkan->createGPUBuffer(_mainCommandPool, _totalInstancesNb * sizeof(GPUObjectInstance),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        objects.data(),
        _gpuObjectInstances
    );

    _vulkan->createGPUBuffer(_mainCommandPool, _totalInstancesNb * sizeof(uint32_t),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        objects.data(),
        _gpuIndexToObjectId
    );


    /*
    * Culling global data buffer
    */

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
    globalData.nbInstances = _totalInstancesNb;

    // Set an arbitrary view matrix into the global data.
    // NOTE: This is used only for debugging the frustum by swapping the player's camera with this one in the culling shader.
    leoscene::Camera camera;
    camera.setPosition(glm::vec3(0.f, 2.5f, 0.f));
    camera.setFront(glm::vec3(0, 0, 1));
    globalData.viewMatrix = glm::lookAt(camera.getPosition(), camera.getPosition() + camera.getFront(), camera.getUp());

    _vulkan->createGPUBuffer(_mainCommandPool, sizeof(GPUCullingGlobalData),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        &globalData,
        _gpuCullingGlobalData
    );


    /*
    * Setup descriptors. Global descriptors depend partially on the scene being loaded, so we create them here.
    */

    _createGlobalDescriptors(_totalInstancesNb);
    _createCullingDescriptors(_totalInstancesNb);


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

    _sceneLoaded = true;
}

void VulkanRenderer::_createGlobalDescriptors(uint32_t _totalInstancesNb)
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
    objectsDataBufferInfo.range = _totalInstancesNb * sizeof(GPUObjectData);

    DescriptorBuilder::begin(_device, _globalDescriptorLayoutCache, _globalDescriptorAllocator)
        .bindBuffer(0, objectsDataBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
        .build(_objectsDataDescriptorSet, _objectsDataDescriptorSetLayout);
}

void VulkanRenderer::_createCullingDescriptors(uint32_t nbObjects)
{
    DescriptorAllocator::Options cullingDescriptorAllocatorOptions = {};
    cullingDescriptorAllocatorOptions.poolBaseSize = 10;
    cullingDescriptorAllocatorOptions.poolSizes = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1.f },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1.f },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 5.f },
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
    instancesInfo.buffer = _gpuObjectInstances.buffer;
    instancesInfo.offset = 0;
    instancesInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo indexMapInfo = {};
    indexMapInfo.buffer = _gpuIndexToObjectId.buffer;
    indexMapInfo.offset = 0;
    indexMapInfo.range = VK_WHOLE_SIZE;

    VkDescriptorImageInfo depthPyramidInfo = {};
    depthPyramidInfo.sampler = _depthImageSampler;
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

    for (uint32_t i = 0; i < _depthPyramid.mipLevels; ++i) {
        VkDescriptorImageInfo srcInfo = {};
        if (i == 0) {
            srcInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            srcInfo.imageView = _depthImage.view;
        }
        else {
            srcInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            srcInfo.imageView = _depthPyramidLevelViews[i - 1];
        }
        srcInfo.sampler = _depthImageSampler;

        VkDescriptorImageInfo dstInfo;
        dstInfo.sampler = _depthImageSampler;
        dstInfo.imageView = _depthPyramidLevelViews[i];
        dstInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        DescriptorBuilder::begin(_device, _globalDescriptorLayoutCache, _depthPyramidDescriptorAllocator)
            .bindImage(0, dstInfo, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
            .bindImage(1, srcInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(_depthPyramidDescriptorSets[i], _depthPyramidDescriptorSetLayout);
    }
}

void VulkanRenderer::_createFramebuffers()
{
    const VulkanInstance::Properties& instanceProperties = _vulkan->getProperties();
    const std::vector<VkImageView>& swapChainImageViews = _vulkan->getSwapChainImageViews();
    VkExtent2D swapChainExtent = _vulkan->getProperties().swapChainExtent;

    /*
    * Images used as framebuffer attachments (excluding the final color targets, which belong to the swapchain created in the instance)
    */

    // Multisampled color attachment (will be resolved into the swap chain images)
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

    // Multisampled depth attachment
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

    // Depth resolve attachment (a depth buffer is required by some shaders and algorithms)
    _vulkan->createImage(swapChainExtent.width, swapChainExtent.height, 1,
        VK_SAMPLE_COUNT_1_BIT,
        _depthBufferFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        _depthImage);
    _vulkan->createImageView(_depthImage.image, _depthBufferFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1, _depthImage.view);

    // Layout of the depth image resolve attachment is initially set to a depth-stencil attachment
    VkCommandBuffer cmd = _vulkan->beginSingleTimeCommands(_mainCommandPool);
    VkImageMemoryBarrier depthLayoutTransitionBarrier = VulkanUtils::createImageBarrier(
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        _depthImage.image,
        VK_IMAGE_ASPECT_DEPTH_BIT,
        0, 0, 0,
        _depthImage.mipLevels
    );
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &depthLayoutTransitionBarrier);
    _vulkan->endSingleTimeCommands(cmd, _mainCommandPool);


    /*
    * Framebuffers
    */

    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        std::array<VkImageView, 4> attachments = {
            _framebufferColor.view,
            _framebufferDepth.view,
            swapChainImageViews[i],
            _depthImage.view
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

void VulkanRenderer::_createDepthPyramid()
{
    const VulkanInstance::Properties& instanceProperties = _vulkan->getProperties();

    _depthPyramidWidth = previousPow2(instanceProperties.swapChainExtent.width);
    _depthPyramidHeight = previousPow2(instanceProperties.swapChainExtent.height);
    uint32_t depthPyramidMipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(instanceProperties.swapChainExtent.width, instanceProperties.swapChainExtent.height)))) + 1;

    _vulkan->createImage(_depthPyramidWidth, _depthPyramidHeight, depthPyramidMipLevels,
        VK_SAMPLE_COUNT_1_BIT,
        VK_FORMAT_R32_SFLOAT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        _depthPyramid);
    _vulkan->createImageView(_depthPyramid.image, VK_FORMAT_R32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, _depthPyramid.mipLevels, _depthPyramid.view);

    // Set initial layout of depth pyramid to general
    VkCommandBuffer cmd = _vulkan->beginSingleTimeCommands(_mainCommandPool);
    VkImageMemoryBarrier depthPyramidLayoutBarrier = VulkanUtils::createImageBarrier(
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL,
        _depthPyramid.image,
        VK_IMAGE_ASPECT_COLOR_BIT,
        0, 0, 0,
        _depthPyramid.mipLevels
    );
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &depthPyramidLayoutBarrier);
    _vulkan->endSingleTimeCommands(cmd, _mainCommandPool);

    _depthPyramidLevelViews.resize(_depthPyramid.mipLevels, VK_NULL_HANDLE);
    for (uint32_t i = 0; i < _depthPyramid.mipLevels; ++i) {
        _vulkan->createImageView(_depthPyramid.image, VK_FORMAT_R32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 1, _depthPyramidLevelViews[i], i);
    }
}

void VulkanRenderer::_createDepthSampler()
{
    // Depth texture sampler with max reduction mode; used to access the single sample depth image or the depth pyramid
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
    reductionCreateInfo.reductionMode = VK_SAMPLER_REDUCTION_MODE_MAX;
    samplerCreateInfo.pNext = &reductionCreateInfo;
    VK_CHECK(vkCreateSampler(_device, &samplerCreateInfo, 0, &_depthImageSampler));
}

void VulkanRenderer::_createBarriers()
{
    // Barriers for layout transition between read and write access of the depth pyramid's levels
    _depthPyramidMipLevelBarriers.resize(_depthPyramid.mipLevels, {});
    for (uint32_t i = 0; i < _depthPyramid.mipLevels; ++i) {
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

    // Barriers for read/write access of the single sampled depth image attached to the framebuffers
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

void VulkanRenderer::setCamera(const leoscene::Camera* camera)
{
    _camera = camera;

    const VkExtent2D& swapChainExtent = _vulkan->getProperties().swapChainExtent;
    _projectionMatrix = glm::perspective(glm::radians(45.0f), static_cast<float>(swapChainExtent.width) / swapChainExtent.height, _zNear, _zFar);
    _invProjectionMatrix = glm::inverse(_projectionMatrix);
}

namespace {
    uint32_t previousPow2(uint32_t v) {
        uint32_t result = 1;
        while (result * 2 < v) result *= 2;
        return result;
    }
}
