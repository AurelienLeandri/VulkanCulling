#include "PipelineBuilder.h"

#include "ShaderPass.h"

#include <iostream>
#include <unordered_map>

VkPipeline PipelineBuilder::buildPipeline(VkDevice device, VkRenderPass pass)
{
	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.pNext = nullptr;

	//no vertex bindings or attributes
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.vertexAttributeDescriptionCount = 0;

	//connect the pipeline builder vertex input info to the one we get from Vertex
	vertexInputInfo.pVertexAttributeDescriptions = vertexAttributes.data();
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributes.size());

	vertexInputInfo.pVertexBindingDescriptions = &vertexBinding;
	vertexInputInfo.vertexBindingDescriptionCount = 1;


	//make viewport state from our stored viewport and scissor.
	//at the moment we wont support multiple viewports or scissors
	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.pNext = nullptr;

	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

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

	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.pNext = nullptr;

	pipelineInfo.stageCount = static_cast<uint32_t>(_shaderStages.size());
	pipelineInfo.pStages = _shaderStages.data();
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.layout = pipelineLayout;
	pipelineInfo.renderPass = pass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

	std::array<VkDynamicState, 3> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE_EXT };
	VkPipelineDynamicStateCreateInfo dynamicStatesInfo{};
	dynamicStatesInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStatesInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicStatesInfo.pDynamicStates = dynamicStates.data();

	pipelineInfo.pDynamicState = &dynamicStatesInfo;

	VkPipeline newPipeline;
	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline) != VK_SUCCESS) {
		std::cerr << "Failed to build graphics pipeline" << std::endl;
		return VK_NULL_HANDLE;
	}

	return newPipeline;
}

void PipelineBuilder::setShaders(const ShaderPass& shaderPass)
{
	const std::unordered_map<VkShaderStageFlagBits, VkShaderModule>& shaderModules = shaderPass.getShaderModules();
	for (auto& [stageFlag, shaderModule] : shaderModules) {
		VkPipelineShaderStageCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		info.pNext = nullptr;
		info.stage = stageFlag;
		info.module = shaderModule;
		info.pName = "main";

		_shaderStages.push_back(info);
	}
}

VkPipeline ComputePipelineBuilder::buildPipeline(VkDevice device)
{
	VkComputePipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineInfo.pNext = nullptr;

	pipelineInfo.stage = shaderStage;
	pipelineInfo.layout = pipelineLayout;


	VkPipeline newPipeline;
	if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline)) {
		std::cerr << "Failed to build compute pipeline" << std::endl;
		return VK_NULL_HANDLE;
	}
	return newPipeline;
}
