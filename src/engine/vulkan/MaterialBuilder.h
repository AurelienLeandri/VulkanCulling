#pragma once

#include "DescriptorUtils.h"
#include "ShaderBuilder.h"
#include "PipelineBuilder.h"
#include "MaterialTemplate.h"
#include "Materials.h"

class Material;

class MaterialBuilder {
public:
	struct Parameters {
		VkDevice device = VK_NULL_HANDLE;
		const VulkanInstance* instance = nullptr;
		VkSampleCountFlagBits multisamplingNbSamples = VK_SAMPLE_COUNT_1_BIT;
		VkRenderPass forwardRenderPass = VK_NULL_HANDLE;
	};

public:
	void init(Parameters parameters);
	void cleanup();
	Material* createMaterial(MaterialType type);
	void setupMaterialDescriptorSets(Material& material);

	const MaterialTemplate* getMaterialTemplate(MaterialType type);

private:
	const VulkanInstance* _vulkan;
	VkDevice _device;
	Parameters _parameters;
	ShaderBuilder _shaderBuilder;

	DescriptorAllocator _descriptorAllocator;
	DescriptorLayoutCache _globalDescriptorLayoutCache;
	std::unordered_map<MaterialType, std::unique_ptr<MaterialTemplate>> _materialTemplates;
	std::vector<std::unique_ptr<Material>> _materials;
};

