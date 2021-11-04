#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include <vector>
#include <array>

// NOTE: Pipeline builder from VkGuide
// https://vkguide.dev/

class GraphicsShaderPass;

class PipelineBuilder {
public:
	VkPipeline buildPipeline(VkDevice device, VkRenderPass pass);
	void setShaders(const GraphicsShaderPass& shaderPass);

public:
	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	VkViewport viewport = {};
	VkRect2D scissor = {};
	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	VkPipelineMultisampleStateCreateInfo multisampling = {};
	VkPipelineLayout pipelineLayout = {};
	VkPipelineDepthStencilStateCreateInfo depthStencil = {};
	VkVertexInputBindingDescription vertexBinding = {};
	std::array<VkVertexInputAttributeDescription, 3> vertexAttributes;

private:
	std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;
};

class VulkanUtils
{

/*
* Initializers
*/

public:
	static VkCommandPoolCreateInfo createCommandPoolInfo(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0);
	static VkCommandBufferAllocateInfo createCommandBufferAllocateInfo(VkCommandPool commandPool, uint32_t nbCommandBuffers, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	static VkPipelineDepthStencilStateCreateInfo createDepthStencilCreateInfo(bool depthTest, bool depthWrite, VkCompareOp compareOp);
};

