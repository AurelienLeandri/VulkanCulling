#include "GraphicShaderPass.h"

#include "ShaderBuilder.h"
#include "DebugUtils.h"

#include "spirv-reflect/spirv_reflect.h"

#include <map>

VkPipelineLayout ShaderPass::reflectShaderModules(const Parameters& parameters)
{
	struct _SetLayoutInfo {
		VkDescriptorSetLayoutCreateInfo createInfo = {};
		std::map<uint32_t, VkDescriptorSetLayoutBinding> bindings;
	};

	std::map<uint32_t, _SetLayoutInfo> reflectedSetLayouts;
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

			_SetLayoutInfo& layoutInfo = reflectedSetLayouts[reflSet.set];

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
	pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(_descriptorSetLayouts.size());
	pipelineLayoutInfo.pSetLayouts = _descriptorSetLayouts.data();
	pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
	pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges.data();

	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VK_CHECK(vkCreatePipelineLayout(_device, &pipelineLayoutInfo, nullptr, &pipelineLayout));

	return pipelineLayout;
}

void ShaderPass::cleanup()
{
	destroyShaderModules();

	for (VkDescriptorSetLayout& descriptorLayout : _descriptorSetLayouts) {
		vkDestroyDescriptorSetLayout(_device, descriptorLayout, nullptr);
	}
	_descriptorSetLayouts.clear();
}

void ShaderPass::destroyShaderModules()
{
	for (auto& [shaderStage, shaderModule] : _shaderModules) {
		vkDestroyShaderModule(_device, shaderModule, nullptr);
	}
	_shaderModules.clear();
}

const std::unordered_map<VkShaderStageFlagBits, VkShaderModule>& ShaderPass::getShaderModules() const
{
	return _shaderModules;
}
