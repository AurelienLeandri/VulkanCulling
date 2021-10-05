#include "VulkanRenderer.h"

#include <Scene/Scene.h>
#include <Scene/Materials/PerformanceMaterial.h>
#include <Scene/Lights/DirectionalLight.h>
#include <Scene/Lights/PointLight.h>
#include <Scene/Geometries/Mesh.h>

#include <iostream>
#include <array>
#include <fstream>

VulkanRenderer::VulkanRenderer(VulkanInstance* vulkan, Options options) :
	_vulkan(vulkan), _options(options), _device(vulkan->getLogicalDevice())
{
}

VulkanRenderer::~VulkanRenderer()
{
    if (_cleanup()) {
        std::cerr << "Error: Cleanup of Vulkan renderer entirely of partially failed" << std::endl;
    }
}

int VulkanRenderer::_cleanup()
{
    // Cleanup of the buffers
    for (auto* buffers : { &vertexBuffers, &indexBuffers }) {
        for (auto& entry : *buffers) {
            for (const _BufferData& bufferData : entry.second) {
                vkDestroyBuffer(_device, bufferData.buffer, nullptr);
                vkFreeMemory(_device, bufferData.memory, nullptr);
            }
            entry.second.clear();
        }
        buffers->clear();
    }

    for (VkBuffer& buffer : _transformsUBOs) {
        vkDestroyBuffer(_device, buffer, nullptr);
    }
    _transformsUBOs.clear();

    for (VkDeviceMemory& memory : _transformsUBOsMemory) {
        vkFreeMemory(_device, memory, nullptr);
    }
    _transformsUBOsMemory.clear();

    // Cleanup of the framebuffers
    vkDestroyImageView(_device, _framebufferColor.view , nullptr);
    vkDestroyImage(_device, _framebufferColor.image, nullptr);
    vkFreeMemory(_device, _framebufferColor.memory, nullptr);
    _framebufferColor = {};

    vkDestroyImageView(_device, _framebufferDepth.view, nullptr);
    vkDestroyImage(_device, _framebufferDepth.image, nullptr);
    vkFreeMemory(_device, _framebufferDepth.memory, nullptr);
    _framebufferDepth = {};

    for (size_t i = 0; i < _framebuffers.size(); i++) {
        vkDestroyFramebuffer(_device, _framebuffers[i], nullptr);
    }
    _framebuffers.clear();

    // Cleanup of the pipeline
    vkDestroyCommandPool(_device, _commandPool, nullptr);
    _commandPool = VK_NULL_HANDLE;

    vkDestroyPipeline(_device, _graphicsPipeline, nullptr);
    _graphicsPipeline = VK_NULL_HANDLE;

    vkDestroyPipelineLayout(_device, _pipelineLayout, nullptr);
    _pipelineLayout = VK_NULL_HANDLE;

    vkDestroyDescriptorSetLayout(_device, _materialDescriptorSetLayout, nullptr);
    _materialDescriptorSetLayout = VK_NULL_HANDLE;

    vkDestroyDescriptorSetLayout(_device, _transformsDescriptorSetLayout, nullptr);
    _transformsDescriptorSetLayout = VK_NULL_HANDLE;

    vkDestroyRenderPass(_device, _renderPass, nullptr);
    _renderPass = VK_NULL_HANDLE;

    return 0;
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

    if (_createCommandPool()) {
        std::cerr << "Could not initialize command pool." << std::endl;
        return -1;
    }

    if (_createFramebufferImageResources() || _createFramebuffers()) {
        std::cerr << "Failed to create framebuffers" << std::endl;
        return -1;
    }

    if (_loadBuffersToDeviceMemory()) {
        std::cerr << "Could not initialize input buffers." << std::endl;
        return -1;
    }

    if (_loadImagesToDeviceMemory()) {
        std::cerr << "Could not initialize input images." << std::endl;
        return -1;
    }

	return 0;
}


int VulkanRenderer::_createCommandPool()
{
    const VulkanInstance::QueueFamilyIndices& queueFamilyIndices = _vulkan->getQueueFamilyIndices();

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
    poolInfo.flags = 0;

    if (vkCreateCommandPool(_device, &poolInfo, nullptr, &_commandPool)) {
        std::cerr << "Failed to create command pool." << std::endl;
        return -1;
    }

    return 0;
}

int VulkanRenderer::_loadBuffersToDeviceMemory()
{
    for (auto& entry : _shapesPerMaterial) {
        const leo::Material* material = entry.first;
        vertexBuffers[material] = std::vector<_BufferData>();
        for (const leo::Shape* shape : entry.second) {
            if (shape->getType() == leo::Shape::Type::MESH) {
                const leo::Mesh* mesh = static_cast<const leo::Mesh*>(shape);

                /*
                * Vertex buffer
                */

                const std::vector<leo::Vertex>& vertices = mesh->vertices;

                VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

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
                memcpy(data, vertices.data(), (size_t)bufferSize);
                vkUnmapMemory(_device, stagingBufferMemory);

                vertexBuffers[material].push_back(_BufferData());
                _BufferData& bufferData = vertexBuffers[material].back();

                if (_createBuffer(bufferSize,
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    bufferData.buffer, bufferData.memory))
                {
                    return -1;
                }

                _copyBuffer(stagingBuffer, bufferData.buffer, bufferSize);

                vkDestroyBuffer(_device, stagingBuffer, nullptr);
                stagingBuffer = VK_NULL_HANDLE;
                vkFreeMemory(_device, stagingBufferMemory, nullptr);
                stagingBufferMemory = VK_NULL_HANDLE;

                /*
                * Index buffer
                */

                const std::vector<uint32_t>& indices = mesh->indices;
                VkDeviceSize indicesBufferSize = sizeof(indices[0]) * indices.size();

                VkBuffer indexStagingBuffer;
                VkDeviceMemory indexStagingBufferMemory;
                if (_createBuffer(indicesBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, indexStagingBuffer, indexStagingBufferMemory))
                {
                    return -1;
                }

                void* indexData = nullptr;
                vkMapMemory(_device, indexStagingBufferMemory, 0, indicesBufferSize, 0, &indexData);
                memcpy(indexData, indices.data(), (size_t)indicesBufferSize);
                vkUnmapMemory(_device, indexStagingBufferMemory);

                indexBuffers[material].push_back(_BufferData());
                _BufferData& indexBufferData = indexBuffers[material].back();

                if (_createBuffer(indicesBufferSize,
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBufferData.buffer, indexBufferData.memory))
                {
                    return -1;
                }

                _copyBuffer(indexStagingBuffer, indexBufferData.buffer, indicesBufferSize);

                vkDestroyBuffer(_device, indexStagingBuffer, nullptr);
                indexStagingBuffer = VK_NULL_HANDLE;
                vkFreeMemory(_device, indexStagingBufferMemory, nullptr);
                indexStagingBufferMemory = VK_NULL_HANDLE;
            }
            else {
                // TODO: Spheres and single triangles?
            }
        }
    }

    /*
    * Transforms UBO
    */

    size_t nbSwapChainImages = _vulkan->getSwapChainImageViews().size();

    VkDeviceSize bufferSize = sizeof(_TransformsUBO);

    _transformsUBOs.resize(nbSwapChainImages);
    _transformsUBOsMemory.resize(nbSwapChainImages);

    for (size_t i = 0; i < nbSwapChainImages; i++) {
        _createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            _transformsUBOs[i], _transformsUBOsMemory[i]);
    }

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
            VkDeviceSize imageSize = texWidth * texHeight * 4;

            VkBuffer stagingBuffer;
            VkDeviceMemory stagingBufferMemory;
            _createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

            void* data;
            vkMapMemory(_device, stagingBufferMemory, 0, imageSize, 0, &data);
            memcpy(data, materialTexture->data, static_cast<size_t>(imageSize));
            vkUnmapMemory(_device, stagingBufferMemory);

            VkFormat imageFormat = VkFormat::VK_FORMAT_UNDEFINED;
            switch (materialTexture->layout) {
            case leo::ImageTexture::Layout::R:
                imageFormat = VK_FORMAT_R8_UNORM;
                break;
            case leo::ImageTexture::Layout::RGB:
                imageFormat = VK_FORMAT_R8G8B8_SRGB;
                break;
            case leo::ImageTexture::Layout::RGBA:
                imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
                break;
            default:
                break;
            }

            if (imageFormat == VkFormat::VK_FORMAT_UNDEFINED) {
                std::cerr << "A texture on a material has a format that is not expected. Something is very very wrong." << std::endl;
                return -1;
            }

            // TODO: Image utils functions
            /*
            _createImage(texWidth, texHeight, vulkanImageData.mipLevels, VK_SAMPLE_COUNT_1_BIT, imageFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vulkanImageData.image, vulkanImageData.memory);

            _transitionImageLayout(vulkanImageData.image, imageFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, vulkanImageData.mipLevels);
            _copyBufferToImage(stagingBuffer, vulkanImageData.image, texWidth, texHeight);

            _generateMipmaps(vulkanImageData.image, imageFormat, texWidth, texHeight, vulkanImageData.mipLevels);
            */

            vkDestroyBuffer(_device, stagingBuffer, nullptr);
            stagingBuffer = VK_NULL_HANDLE;
            vkFreeMemory(_device, stagingBufferMemory, nullptr);
            stagingBufferMemory = VK_NULL_HANDLE;
        }
    }

    return 0;
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
    VkCommandBuffer commandBuffer = _beginSingleTimeCommands(_commandPool);

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    _endSingleTimeCommands(commandBuffer, _commandPool);

    return 0;
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
    pipelineLayoutInfo.setLayoutCount = 2;
    std::array<VkDescriptorSetLayout, 2> pSetLayouts = { _transformsDescriptorSetLayout, _materialDescriptorSetLayout };
    pipelineLayoutInfo.pSetLayouts = pSetLayouts.data();

    VkPushConstantRange pushConstants = {};
    pushConstants.size = sizeof (glm::mat4);
    pushConstants.offset = 0;
    pushConstants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstants;

    if (vkCreatePipelineLayout(_device, &pipelineLayoutInfo, nullptr, &_pipelineLayout)) {
        std::cerr << "Failed to create pipeline layout." << std::endl;
        return -1;
    }

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.minDepthBounds = 0.0f; // Optional
    depthStencil.maxDepthBounds = 1.0f; // Optional
    depthStencil.stencilTestEnable = VK_FALSE;
    depthStencil.front = {}; // Optional
    depthStencil.back = {}; // Optional

    VkGraphicsPipelineCreateInfo pipelineInfo{};
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

void VulkanRenderer::_constructSceneRelatedStructures()
{
    for (const leo::SceneObject& sceneObject : _scene->objects) {
        const leo::Material* material = sceneObject.material.get();
        const leo::Shape* shape = sceneObject.shape.get();
        const leo::Transform* transform = sceneObject.transform.get();  // No per scene object transform. No point in that since the scene doesnt move. We must pre-transform objects.
        if (_shapesPerMaterial.find(material) == _shapesPerMaterial.end()) {
            _shapesPerMaterial[material] = std::vector<const leo::Shape*>();
        }
        if (_materialsImages.find(material) == _materialsImages.end()) {
            _materialsImages[material] = std::vector<_ImageData>(5);
        }
        _shapesPerMaterial[material].push_back(shape);
    }
}

int VulkanRenderer::_createDescriptorSetLayouts()
{
    /*
    * Transforms
    */

    VkDescriptorSetLayoutBinding uboTransformsLayoutBinding = {};
    uboTransformsLayoutBinding.binding = 0;
    uboTransformsLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboTransformsLayoutBinding.descriptorCount = 1;
    uboTransformsLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    uboTransformsLayoutBinding.pImmutableSamplers = nullptr;


    VkDescriptorSetLayoutCreateInfo uboTransformsLayoutInfo = {};
    uboTransformsLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    uboTransformsLayoutInfo.bindingCount = 1;
    uboTransformsLayoutInfo.pBindings = &uboTransformsLayoutBinding;

    if (vkCreateDescriptorSetLayout(_device, &uboTransformsLayoutInfo, nullptr, &_transformsDescriptorSetLayout)) {
        std::cerr << "Failed to create transforms descriptor set layout." << std::endl;
        return -1;
    }

    /*
    * Material layout
    */

    static const uint32_t nbTexturesInMaterial = 5;

    std::array<VkDescriptorSetLayoutBinding, nbTexturesInMaterial> bindings = { {} };

    for (uint32_t i = 0; i < nbTexturesInMaterial; ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorCount = 1;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[i].pImmutableSamplers = nullptr;
        bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    VkDescriptorSetLayoutCreateInfo materialLayoutInfo{};
    materialLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    materialLayoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    materialLayoutInfo.pBindings = bindings.data();

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

    _framebufferColor.view = _vulkan->createImageView(_framebufferColor.image, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);

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

    _framebufferDepth.view = _vulkan->createImageView(_framebufferDepth.image, _depthBufferFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);

    return 0;
}

int VulkanRenderer::_createFramebuffers()
{
    const std::vector<VkImageView>& swapChainImageViews = _vulkan->getSwapChainImageViews();

    _framebuffers.resize(swapChainImageViews.size());

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

        if (vkCreateFramebuffer(_device, &framebufferInfo, nullptr, &_framebuffers[i])) {
            std::cerr << "Failed to create framebuffer." << std::endl;
            return -1;
        }
    }
    return 0;
}

VkCommandBuffer VulkanRenderer::_beginSingleTimeCommands(VkCommandPool& commandPool) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(_device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
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
