#pragma once

#include "VulkanInstance.h"

#include <memory>
#include <unordered_map>
#include <map>

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
	struct _ImageData {
		VkImage image = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
		VkImageView view = VK_NULL_HANDLE;
		VkSampler textureSampler = VK_NULL_HANDLE;
		uint32_t mipLevels = 0;
	};

	struct _TransformsUBO {
		alignas(16) glm::mat4 model;
		alignas(16) glm::mat4 proj;
	};

	struct _PushConstants {
		alignas(16) glm::mat4 view;
	};

	struct _BufferData {
		VkBuffer buffer = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
		size_t nbElements = 0;
	};

	struct _DescriptorSets {
		VkDescriptorSet _materialDescriptorSet;
		VkDescriptorSet _transformsDescriptorSet;
	};

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
	int _createDescriptorPools();
	int _createDescriptorSets();
	int _createCommandBuffers();

	int _createShaderModule(const char* glslFilePath, VkShaderModule& shaderModule);

	int _createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
		VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	int _copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

	int _transitionImageLayout(_ImageData& imageData, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
	void _copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
	void _generateMipmaps(_ImageData& imageData, VkFormat imageFormat, int32_t texWidth, int32_t texHeight);

	VkCommandBuffer _beginSingleTimeCommands(VkCommandPool& commandPool);
	void _endSingleTimeCommands(VkCommandBuffer commandBuffer, VkCommandPool& commandPool);

	int _cleanup();

private:
	const leo::Scene* _scene = nullptr;
	VulkanInstance* _vulkan = nullptr;
	VkDevice _device = VK_NULL_HANDLE;
	Options _options;
	std::map<const leo::Material*, std::vector<const leo::Shape*>> _shapesPerMaterial;

	VkCommandPool _commandPool = VK_NULL_HANDLE;

	VkFormat _depthBufferFormat = VK_FORMAT_UNDEFINED;

	VkDescriptorSetLayout _materialDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorSetLayout _transformsDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool _descriptorPool = VK_NULL_HANDLE;

	VkPipelineLayout _pipelineLayout = VK_NULL_HANDLE;
	VkRenderPass _renderPass = VK_NULL_HANDLE;
	VkPipeline _graphicsPipeline = VK_NULL_HANDLE;

	_ImageData _framebufferColor;
	_ImageData _framebufferDepth;
	std::vector<VkFramebuffer> _framebuffers;

	std::vector<VkBuffer> _transformsUBOs;
	std::vector<VkDeviceMemory> _transformsUBOsMemory;

	std::vector<VkDescriptorSet> _materialDescriptorSets;  // One entry per swapchain image
	std::vector<VkDescriptorSet> _transformsDescriptorSets;  // One entry per swapchain image

	std::vector<VkCommandBuffer> _commandBuffers;

	std::unordered_map<const leo::Material*, std::vector<_BufferData>> _vertexBuffers;
	std::unordered_map<const leo::Material*, std::vector<_BufferData>> _indexBuffers;

	// Order within each vector: diffuse, specular, ambient, normals, height
	std::unordered_map<const leo::Material*, std::vector<_ImageData>> _materialsImages;
};

