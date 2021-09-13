#include "VulkanRenderer.h"

#include <Scene/Scene.h>
#include <Scene/Materials/PerformanceMaterial.h>
#include <Scene/Lights/DirectionalLight.h>
#include <Scene/Lights/PointLight.h>
#include <Scene/Geometries/Mesh.h>

#include <iostream>

VulkanRenderer::VulkanRenderer(VulkanInstance* vulkan, Options options) :
	_vulkan(vulkan), _options(options), _device(vulkan->getLogicalDevice())
{
}

int VulkanRenderer::init()
{
	for (const leo::SceneObject& sceneObject : _scene->objects) {
		const leo::Material* material = sceneObject.material.get();
		const leo::Shape* shape = sceneObject.shape.get();
		const leo::Transform* transform = sceneObject.transform.get();  // No per scene object transform. No point in that since the scene doesnt move. We must pre-transform objects.
		if (_shapesPerMaterial.find(material) == _shapesPerMaterial.end()) {
			_shapesPerMaterial[material] = std::vector<const leo::Shape*>();
		}
		_shapesPerMaterial[material].push_back(shape);
	}

    if (_createCommandPool()) {
        return -1;
    }

    if (_createInputBuffers()) {
        std::cerr << "Could not initialize input buffers." << std::endl;
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

int VulkanRenderer::_createInputBuffers()
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
