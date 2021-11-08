#include "Materials.h"

#include "DebugUtils.h"
#include "Scene/Geometries/Vertex.h"
#include "DescriptorUtils.h"

#include "spirv-reflect/spirv_reflect.h"

#include <map>
#include <array>


void MaterialTemplate::init(const Parameters& parameters)
{
	_device = parameters.device;
	for (const auto& [passType, passParameters] : parameters.passesParameters) {
		_shaderPasses[passType] = std::make_unique<GraphicsShaderPass>();
		_shaderPasses[passType]->init(passParameters);

	}
}

const GraphicsShaderPass* MaterialTemplate::getShaderPass(GraphicsShaderPassType passType) const
{
	return _shaderPasses.at(passType).get();
}

GraphicsShaderPass* MaterialTemplate::getShaderPass(GraphicsShaderPassType passType)
{
	if (_shaderPasses.find(passType) == _shaderPasses.end()) {
		return nullptr;
	}

	return _shaderPasses[passType].get();
}

void GraphicsShaderPass::init(const Parameters& parameters)
{
	static struct SetLayoutInfo {
		VkDescriptorSetLayoutCreateInfo createInfo = {};
		std::map<uint32_t, VkDescriptorSetLayoutBinding> bindings;
	};

	std::map<uint32_t, SetLayoutInfo> reflectedSetLayouts;
	std::vector<VkPushConstantRange> pushConstantRanges;

	_device = parameters.device;
	_shaderBuilder = parameters.shaderBuilder;
	for (const auto& [stageFlag, path] : parameters.shaderPaths) {
		std::vector<char> buffer;
		_shaderModules[stageFlag] = {};
		_shaderBuilder->createShaderModule(path, _shaderModules[stageFlag], &buffer);
		VkShaderModule shaderModule = _shaderModules[stageFlag];

		SpvReflectShaderModule spvmodule;
		SpvReflectResult result = spvReflectCreateShaderModule(buffer.size(), buffer.data(), &spvmodule);

		/*
		* Descriptor layouts
		*/

		uint32_t count = 0;
		if (spvReflectEnumerateDescriptorSets(&spvmodule, &count, NULL)) {
			throw VulkanRendererException("Error: spvReflectEnumerateDescriptorSets");
		}

		std::vector<SpvReflectDescriptorSet*> sets(count);
		if (spvReflectEnumerateDescriptorSets(&spvmodule, &count, sets.data())) {
			throw VulkanRendererException("Error: spvReflectEnumerateDescriptorSets");
		}

		for (size_t setIdx = 0; setIdx < sets.size(); ++setIdx) {
			SpvReflectDescriptorSet& reflSet = *sets[setIdx];

			bool setDuplicate = reflectedSetLayouts.find(reflSet.set) != reflectedSetLayouts.end();
			if (!setDuplicate) {
				reflectedSetLayouts[reflSet.set] = {};
				reflectedSetLayouts[reflSet.set].createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			}

			SetLayoutInfo& layoutInfo = reflectedSetLayouts[reflSet.set];

			std::map<uint32_t, VkDescriptorSetLayoutBinding>& bindings = layoutInfo.bindings;
			for (size_t i = 0; i < reflSet.binding_count; ++i) {
				const SpvReflectDescriptorBinding& reflBinding = *reflSet.bindings[i];
				uint32_t bindingIdx = reflBinding.binding;

				bool bindingDuplicate = bindings.find(bindingIdx) != bindings.end();
				if (!bindingDuplicate) {
					bindings[bindingIdx] = {};
					bindings[bindingIdx].binding = bindingIdx;
					if (parameters.descriptorTypeOverwrites.find(reflBinding.name) != parameters.descriptorTypeOverwrites.end()) {
						bindings[bindingIdx].descriptorType = parameters.descriptorTypeOverwrites.at(reflBinding.name);
					}
					else {
						bindings[bindingIdx].descriptorType = static_cast<VkDescriptorType>(reflBinding.descriptor_type);
					}
					bindings[bindingIdx].descriptorCount = 1;
					for (uint32_t dimIdx = 0; dimIdx < reflBinding.array.dims_count; ++dimIdx) {
						bindings[bindingIdx].descriptorCount *= reflBinding.array.dims[dimIdx];
					}
					bindings[bindingIdx].stageFlags = stageFlag;
					bindings[bindingIdx].pImmutableSamplers = nullptr;
				}
				else {
					bindings[bindingIdx].stageFlags |= stageFlag;
				}
			}
		}

		/*
		* Push Constants
		*/

		if (spvReflectEnumeratePushConstantBlocks(&spvmodule, &count, NULL)) {
			throw VulkanRendererException("Error: spvReflectEnumeratePushConstantBlocks");
		}

		std::vector<SpvReflectBlockVariable*> reflectedPushConstants(count);
		if (spvReflectEnumeratePushConstantBlocks(&spvmodule, &count, reflectedPushConstants.data())) {
			throw VulkanRendererException("Error: spvReflectEnumeratePushConstantBlocks");
		}

		if (count > 0) {
			VkPushConstantRange pcs{};
			pcs.offset = reflectedPushConstants[0]->offset;
			pcs.size = reflectedPushConstants[0]->size;
			pcs.stageFlags = stageFlag;

			pushConstantRanges.push_back(pcs);
		}
	}

	this->_descriptorSetLayouts.resize(reflectedSetLayouts.size(), VK_NULL_HANDLE);
	for (auto& [setIdx, layoutInfo] : reflectedSetLayouts) {
		VkDescriptorSetLayoutCreateInfo& createInfo = layoutInfo.createInfo;
		createInfo.bindingCount = static_cast<uint32_t>(layoutInfo.bindings.size());
		std::vector<VkDescriptorSetLayoutBinding> bindings(layoutInfo.bindings.size());
		for (auto& [bindingIdx, layoutBinding] : layoutInfo.bindings) {
			bindings[bindingIdx] = layoutBinding;
		}
		createInfo.pBindings = bindings.data();

		VK_CHECK(vkCreateDescriptorSetLayout(_device, &createInfo, nullptr, &_descriptorSetLayouts[setIdx]));
	}


	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = _descriptorSetLayouts.size();
	pipelineLayoutInfo.pSetLayouts = _descriptorSetLayouts.data();
	pipelineLayoutInfo.pushConstantRangeCount = pushConstantRanges.size();
	pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges.data();

	VK_CHECK(vkCreatePipelineLayout(_device, &pipelineLayoutInfo, nullptr, &_pipelineLayout));
}

void GraphicsShaderPass::setPipeline(VkPipeline pipeline)
{
	_pipeline = pipeline;
}

const VkPipeline GraphicsShaderPass::getPipeline() const
{
	return _pipeline;
}

VkPipelineLayout GraphicsShaderPass::getPipelineLayout() const
{
	return _pipelineLayout;
}

const std::unordered_map<VkShaderStageFlagBits, VkShaderModule>& GraphicsShaderPass::getShaderModules() const
{
	return _shaderModules;
}

MaterialBuilder::MaterialBuilder(VkDevice device)
	: _device(device), _shaderBuilder(_device), _descriptorAllocator(_device), _descriptorLayoutCache(_device)
{
	DescriptorAllocator::Options descriptorAllocatorOptions = {};
	descriptorAllocatorOptions.poolBaseSize = 10;
	descriptorAllocatorOptions.poolSizes = {
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5.f },
	};
	_descriptorAllocator.init(descriptorAllocatorOptions);
}

void MaterialBuilder::init(Parameters parameters)
{
	_parameters = parameters;

	/*
	* Graphics pipelines
	*/

	// Forward pipeline

	VkVertexInputBindingDescription bindingDescription = {};
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof(leo::Vertex);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

	attributeDescriptions[0].binding = 0;
	attributeDescriptions[0].location = 0;
	attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[0].offset = offsetof(leo::Vertex, position);

	attributeDescriptions[1].binding = 0;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[1].offset = offsetof(leo::Vertex, normal);

	attributeDescriptions[2].binding = 0;
	attributeDescriptions[2].location = 2;
	attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescriptions[2].offset = offsetof(leo::Vertex, uv);
	_forwardPipelineBuilder.vertexAttributes = attributeDescriptions;
	_forwardPipelineBuilder.vertexBinding = bindingDescription;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
	inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyInfo.pNext = nullptr;
	inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;
	_forwardPipelineBuilder.inputAssembly = inputAssemblyInfo;

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
	_forwardPipelineBuilder.rasterizer = rasterizerInfo;

	VkPipelineMultisampleStateCreateInfo multisamplingInfo{};
	multisamplingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisamplingInfo.pNext = nullptr;

	multisamplingInfo.sampleShadingEnable = VK_FALSE;
	multisamplingInfo.rasterizationSamples = _parameters.multisamplingNbSamples;
	multisamplingInfo.minSampleShading = 1.0f;
	multisamplingInfo.pSampleMask = nullptr;
	multisamplingInfo.alphaToCoverageEnable = VK_FALSE;
	multisamplingInfo.alphaToOneEnable = VK_FALSE;
	_forwardPipelineBuilder.multisampling = multisamplingInfo;

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
	_forwardPipelineBuilder.colorBlendAttachment = colorBlendAttachment;

	VkPipelineDepthStencilStateCreateInfo depthStencilInfo = {};
	depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilInfo.pNext = nullptr;
	depthStencilInfo.depthTestEnable = VK_TRUE;
	depthStencilInfo.depthWriteEnable = VK_TRUE;
	depthStencilInfo.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
	depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
	depthStencilInfo.minDepthBounds = 0.0f; // Optional
	depthStencilInfo.maxDepthBounds = 1.0f; // Optional
	depthStencilInfo.stencilTestEnable = VK_FALSE;
	_forwardPipelineBuilder.depthStencil = depthStencilInfo;


	/*
	* Performance material template
	*/

	MaterialTemplate::Parameters performanceMaterialTemplateParams{};
	performanceMaterialTemplateParams.device = _device;
	performanceMaterialTemplateParams.shaderBuilder = &_shaderBuilder;

	// Forward pass

	GraphicsShaderPass::Parameters forwardPassParams{};
	forwardPassParams.device = _device;
	forwardPassParams.shaderBuilder = &_shaderBuilder;
	forwardPassParams.shaderPaths[VK_SHADER_STAGE_VERTEX_BIT] = "../Resources/Shaders/vert.spv";
	forwardPassParams.shaderPaths[VK_SHADER_STAGE_FRAGMENT_BIT] = "../Resources/Shaders/frag.spv";
	forwardPassParams.descriptorTypeOverwrites["transforms"] = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	performanceMaterialTemplateParams.passesParameters[GraphicsShaderPassType::FORWARD] = forwardPassParams;
	_forwardPassTemplate.init(performanceMaterialTemplateParams);
	_forwardPipelineBuilder.pipelineLayout = _forwardPassTemplate.getShaderPass(GraphicsShaderPassType::FORWARD)->getPipelineLayout();

	_forwardPipelineBuilder.setShaders(*_forwardPassTemplate.getShaderPass(GraphicsShaderPassType::FORWARD));
	VkPipeline forwardPassPipeline = _forwardPipelineBuilder.buildPipeline(_device, _parameters.forwardRenderPass);
	_forwardPassTemplate.getShaderPass(GraphicsShaderPassType::FORWARD)->setPipeline(forwardPassPipeline);
}

Material* MaterialBuilder::createMaterial(MaterialType type)
{
	_materials.push_back(std::make_unique<Material>(this, &_forwardPassTemplate));
	return _materials.back().get();
}

void MaterialBuilder::setupMaterialDescriptorSet(Material& material)
{
	DescriptorBuilder builder = DescriptorBuilder::begin(_device, _descriptorLayoutCache, _descriptorAllocator);
	std::array<VkDescriptorImageInfo, 5> imageInfos = { {} };
	for (int i = 0; i < 5; ++i) {
		imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfos[i].imageView = material.textures[i].view;
		imageInfos[i].sampler = material.textures[i].sampler;

		builder.bindImage(i, imageInfos[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	}

	builder.build(material.descriptorSets[GraphicsShaderPassType::FORWARD]);
}

Material::Material(MaterialBuilder* builder, const MaterialTemplate* materialTemplate) :
	_builder(builder), _materialTemplate(materialTemplate)
{
}

void Material::initDescriptorSet()
{
	_builder->setupMaterialDescriptorSet(*this);
	
}

const MaterialTemplate* Material::getTemplate() const
{
	return _materialTemplate;
}
