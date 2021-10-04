#pragma once

#include "VulkanInstance.h"

#include <memory>
#include <unordered_map>

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
	int _createCommandPool();
	int _createInputBuffers();
	int _createDescriptorSetLayout();
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

	VkDescriptorSetLayout _descriptorSetLayout = VK_NULL_HANDLE;
	VkPipelineLayout _pipelineLayout = VK_NULL_HANDLE;
	VkRenderPass _renderPass = VK_NULL_HANDLE;
	VkPipeline _graphicsPipeline = VK_NULL_HANDLE;

	std::vector<VkFramebuffer> _framebuffers;
	VkImage _framebufferColor;
	VkDeviceMemory _framebufferColorMemory;
	VkImageView _framebufferColorView;
	VkImage _framebufferDepth;
	VkDeviceMemory _framebufferDepthMemory;
	VkImageView _framebufferDepthView;

	struct _BufferData {
		VkBuffer buffer = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
	};
	std::unordered_map<const leo::Material*, std::vector<_BufferData>> vertexBuffers;
	std::unordered_map<const leo::Material*, std::vector<_BufferData>> indexBuffers;
};

