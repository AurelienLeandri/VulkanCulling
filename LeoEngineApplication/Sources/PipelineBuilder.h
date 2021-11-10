#pragma once

#include "vulkan/vulkan.h"

#include <array>
#include <vector>

// NOTE: Pipeline builder from VkGuide
// https://vkguide.dev/

class GraphicShaderPass;

class PipelineBuilder {
public:
	VkPipeline buildPipeline(VkDevice device, VkRenderPass pass);
	void setShaders(const GraphicShaderPass& shaderPass);

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

