#include "Materials.h"

#include "DebugUtils.h"

#include "spirv-reflect/spirv_reflect.h"


void MaterialTemplate::init(const Parameters& parameters)
{
	_device = parameters.device;
	_descriptorAllocator = parameters.descriptorAllocator;
	_descriptorLayoutCache = parameters.descriptorLayoutCache;
	for (const auto& [passType, passParameters] : parameters.passesParameters) {
		_shaderPasses[passType] = {};
		_shaderPasses[passType].init(passParameters);

	}
}

void GraphicsShaderPass::init(const Parameters& parameters)
{
	_device = parameters.device;
	_descriptorAllocator = parameters.descriptorAllocator;
	_descriptorLayoutCache = parameters.descriptorLayoutCache;
	_shaderBuilder = parameters.shaderBuilder;
	for (const auto& [stageFlag, path] : parameters.shaderPaths) {
		std::vector<char> buffer;
		_shaderModules[stageFlag] = {};
		_shaderBuilder->createShaderModule(path, _shaderModules[stageFlag], &buffer);
		VkShaderModule shaderModule = _shaderModules[stageFlag];

		SpvReflectShaderModule spvmodule;
		SpvReflectResult result = spvReflectCreateShaderModule(buffer.size(), buffer.data(), &spvmodule);

		uint32_t count = 0;
		if (spvReflectEnumerateDescriptorSets(&spvmodule, &count, NULL)) {
			throw VulkanRendererException("Error: spvReflectEnumerateDescriptorSets");
		}

		std::vector<SpvReflectDescriptorSet*> sets(count);
		if (spvReflectEnumerateDescriptorSets(&spvmodule, &count, NULL)) {
			throw VulkanRendererException("Error: spvReflectEnumerateDescriptorSets");
		}

		for (size_t setIdx = 0; setIdx < sets.size(); ++setIdx) {

			const SpvReflectDescriptorSet& refl_set = *(sets[setIdx]);
			/*
			DescriptorSetLayoutData layout = {};

			layout.bindings.resize(refl_set.binding_count);
			for (uint32_t i_binding = 0; i_binding < refl_set.binding_count; ++i_binding) {
				const SpvReflectDescriptorBinding& refl_binding = *(refl_set.bindings[i_binding]);
				VkDescriptorSetLayoutBinding& layout_binding = layout.bindings[i_binding];
				layout_binding.binding = refl_binding.binding;
				layout_binding.descriptorType = static_cast<VkDescriptorType>(refl_binding.descriptor_type);

				for (int ov = 0; ov < overrideCount; ov++)
				{
					if (strcmp(refl_binding.name, overrides[ov].name) == 0) {
						layout_binding.descriptorType = overrides[ov].overridenType;
					}
				}

				layout_binding.descriptorCount = 1;
				for (uint32_t i_dim = 0; i_dim < refl_binding.array.dims_count; ++i_dim) {
					layout_binding.descriptorCount *= refl_binding.array.dims[i_dim];
				}
				layout_binding.stageFlags = static_cast<VkShaderStageFlagBits>(spvmodule.shader_stage);

				ReflectedBinding reflected;
				reflected.binding = layout_binding.binding;
				reflected.set = refl_set.set;
				reflected.type = layout_binding.descriptorType;

				bindings[refl_binding.name] = reflected;
			}
			layout.set_number = refl_set.set;
			layout.create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			layout.create_info.bindingCount = refl_set.binding_count;
			layout.create_info.pBindings = layout.bindings.data();

			set_layouts.push_back(layout);
		}
		*/
		}
	}
}
