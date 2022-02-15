#include "MaterialBuilder.h"

#include "VulkanInstance.h"

#include <scene/Vertex.h>

#include <cstddef>


void MaterialBuilder::init(Parameters parameters)
{
	_device = parameters.device;
	_vulkan = parameters.instance;

	_shaderBuilder.init(_device);
	_descriptorAllocator.init(_device);
	_globalDescriptorLayoutCache.init(_device);

	DescriptorAllocator::Options descriptorAllocatorOptions = {};
	descriptorAllocatorOptions.poolBaseSize = 10;
	descriptorAllocatorOptions.poolSizes = {
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5.f },
	};
	_descriptorAllocator.init(_device, descriptorAllocatorOptions);

	_parameters = parameters;

	PipelineBuilder forwardPipelineBuilder;

	/*
	* Graphics pipelines
	*/

	// Forward pipeline

	VkVertexInputBindingDescription bindingDescription = {};
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof(leoscene::Vertex);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

	attributeDescriptions[0].binding = 0;
	attributeDescriptions[0].location = 0;
	attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[0].offset = offsetof(leoscene::Vertex, position);

	attributeDescriptions[1].binding = 0;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[1].offset = offsetof(leoscene::Vertex, normal);

	attributeDescriptions[2].binding = 0;
	attributeDescriptions[2].location = 2;
	attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescriptions[2].offset = offsetof(leoscene::Vertex, uv);
	forwardPipelineBuilder.vertexAttributes = attributeDescriptions;
	forwardPipelineBuilder.vertexBinding = bindingDescription;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
	inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyInfo.pNext = nullptr;
	inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;
	forwardPipelineBuilder.inputAssembly = inputAssemblyInfo;

	VkPipelineRasterizationStateCreateInfo rasterizerInfo{};
	rasterizerInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizerInfo.pNext = nullptr;
	rasterizerInfo.depthClampEnable = VK_FALSE;
	rasterizerInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizerInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizerInfo.lineWidth = 1.0f;
	rasterizerInfo.cullMode = VK_CULL_MODE_NONE;
	rasterizerInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizerInfo.depthBiasEnable = VK_FALSE;
	rasterizerInfo.depthBiasConstantFactor = 0.0f;
	rasterizerInfo.depthBiasClamp = 0.0f;
	rasterizerInfo.depthBiasSlopeFactor = 0.0f;
	forwardPipelineBuilder.rasterizer = rasterizerInfo;

	VkPipelineMultisampleStateCreateInfo multisamplingInfo{};
	multisamplingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisamplingInfo.pNext = nullptr;

	multisamplingInfo.sampleShadingEnable = VK_FALSE;
	multisamplingInfo.rasterizationSamples = _parameters.multisamplingNbSamples;
	multisamplingInfo.minSampleShading = 0;
	multisamplingInfo.pSampleMask = nullptr;
	multisamplingInfo.alphaToCoverageEnable = VK_FALSE;
	multisamplingInfo.alphaToOneEnable = VK_FALSE;
	forwardPipelineBuilder.multisampling = multisamplingInfo;

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
	forwardPipelineBuilder.colorBlendAttachment = colorBlendAttachment;

	forwardPipelineBuilder.depthStencil = VulkanUtils::createDepthStencilCreateInfo(true, true, VK_COMPARE_OP_LESS);

	const VulkanInstance::Properties& instanceProperties = _vulkan->getProperties();

	VkViewport viewport{};
	forwardPipelineBuilder.viewport.x = 0.0f;
	forwardPipelineBuilder.viewport.y = 0.0f;
	forwardPipelineBuilder.viewport.width = static_cast<float>(instanceProperties.swapChainExtent.width);
	forwardPipelineBuilder.viewport.height = static_cast<float>(instanceProperties.swapChainExtent.height);
	forwardPipelineBuilder.viewport.minDepth = 0.0f;
	forwardPipelineBuilder.viewport.maxDepth = 1.0f;

	VkRect2D scissor{};
	forwardPipelineBuilder.scissor.offset = { 0, 0 };
	forwardPipelineBuilder.scissor.extent = instanceProperties.swapChainExtent;

	/*
	* Performance material template
	*/

	MaterialTemplate::Parameters performanceMaterialTemplateParams{};
	performanceMaterialTemplateParams.device = _device;
	performanceMaterialTemplateParams.shaderBuilder = &_shaderBuilder;

	// Forward pass

	ShaderPass::Parameters forwardPassParams{};
	forwardPassParams.device = _device;
	forwardPassParams.shaderBuilder = &_shaderBuilder;
	forwardPassParams.shaderPaths[VK_SHADER_STAGE_VERTEX_BIT] = "resources/shaders/vert.spv";
	forwardPassParams.shaderPaths[VK_SHADER_STAGE_FRAGMENT_BIT] = "resources/shaders/frag.spv";

	_materialTemplates[MaterialType::BASIC] = std::make_unique<MaterialTemplate>();
	performanceMaterialTemplateParams.passesParameters[ShaderPass::Type::FORWARD] = forwardPassParams;

	_materialTemplates[MaterialType::BASIC]->init(performanceMaterialTemplateParams);
	forwardPipelineBuilder.pipelineLayout = _materialTemplates[MaterialType::BASIC]->getPipelineLayout(ShaderPass::Type::FORWARD);

	forwardPipelineBuilder.setShaders(*_materialTemplates[MaterialType::BASIC]->getShaderPass(ShaderPass::Type::FORWARD));
	VkPipeline forwardPassPipeline = forwardPipelineBuilder.buildPipeline(_device, _parameters.forwardRenderPass);
	_materialTemplates[MaterialType::BASIC]->setPipeline(ShaderPass::Type::FORWARD, forwardPassPipeline);

	_materialTemplates[MaterialType::BASIC]->getShaderPass(ShaderPass::Type::FORWARD)->destroyShaderModules();
}

void MaterialBuilder::cleanup()
{
	_descriptorAllocator.cleanup();
	_globalDescriptorLayoutCache.cleanup();
	_materials.clear();
	for (auto& [materialType, materialTemplate] : _materialTemplates) {
		materialTemplate->cleanup();
	}
	_materialTemplates.clear();
}

Material* MaterialBuilder::createMaterial(MaterialType type)
{
	_materials.push_back(std::make_unique<Material>(this, _materialTemplates[type].get(), type));
	return _materials.back().get();
}

void MaterialBuilder::setupMaterialDescriptorSets(Material& material)
{
	DescriptorBuilder builder = DescriptorBuilder::begin(_device, _globalDescriptorLayoutCache, _descriptorAllocator);
	std::array<VkDescriptorImageInfo, 5> imageInfos = { {} };
	for (int i = 0; i < 5; ++i) {
		imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfos[i].imageView = material.textures[i].view;
		imageInfos[i].sampler = material.textures[i].sampler;

		builder.bindImage(i, imageInfos[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	}

	builder.build(material.getDescriptorSet(ShaderPass::Type::FORWARD));
}

const MaterialTemplate* MaterialBuilder::getMaterialTemplate(MaterialType type)
{
	return _materialTemplates.at(type).get();
}
