#pragma once

#include "VulkanInstance.h"

#include <memory>
#include <unordered_map>

#include <glm/glm.hpp>

namespace leo {
	class Scene;
	class Material;
	class Shape;
}


class VulkanRenderer
{
public:
	struct Options {
	};

	VulkanRenderer(VulkanInstance* vulkan, Options options = {});
	~VulkanRenderer();

public:
	void setScene(const leo::Scene* scene);
	int init();

private:
	void _constructSceneRelatedStructures();
	int _createCommandPool();
	int _loadBuffersToDeviceMemory();
	int _loadImagesToDeviceMemory();
	int _createDescriptorSetLayouts();
	int _createRenderPass();
	int _createGraphicsPipeline();
	int _createFramebufferImageResources();
	int _createFramebuffers();

	int _createShaderModule(const char* glslFilePath, VkShaderModule& shaderModule);
	int _createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
		VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	int _copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
	VkCommandBuffer _beginSingleTimeCommands(VkCommandPool& commandPool);
	void _endSingleTimeCommands(VkCommandBuffer commandBuffer, VkCommandPool& commandPool);

	int _cleanup();

private:
	const leo::Scene* _scene = nullptr;
	VulkanInstance* _vulkan = nullptr;
	VkDevice _device = VK_NULL_HANDLE;
	Options _options;
	std::unordered_map<const leo::Material*, std::vector<const leo::Shape*>> _shapesPerMaterial;

	VkCommandPool _commandPool = VK_NULL_HANDLE;

	VkFormat _depthBufferFormat = VK_FORMAT_UNDEFINED;

	VkDescriptorSetLayout _materialDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorSetLayout _transformsDescriptorSetLayout = VK_NULL_HANDLE;
	VkPipelineLayout _pipelineLayout = VK_NULL_HANDLE;
	VkRenderPass _renderPass = VK_NULL_HANDLE;
	VkPipeline _graphicsPipeline = VK_NULL_HANDLE;


	struct _ImageData {
		VkImage image = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
		VkImageView view = VK_NULL_HANDLE;
		uint32_t mipLevels = 0;
	};
	_ImageData _framebufferColor;
	_ImageData _framebufferDepth;
	std::vector<VkFramebuffer> _framebuffers;

	struct _TransformsUBO {
		alignas(16) glm::mat4 model;
		alignas(16) glm::mat4 proj;
	};
	std::vector<VkBuffer> _transformsUBOs;
	std::vector<VkDeviceMemory> _transformsUBOsMemory;

	struct _PushConstants {
		alignas(16) glm::mat4 view;
	};

	struct _BufferData {
		VkBuffer buffer = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
	};
	std::unordered_map<const leo::Material*, std::vector<_BufferData>> vertexBuffers;
	std::unordered_map<const leo::Material*, std::vector<_BufferData>> indexBuffers;

	// Order within each vector: diffuse, specular, ambient, normals, height
	std::unordered_map<const leo::Material*, std::vector<_ImageData>> _materialsImages;
};

