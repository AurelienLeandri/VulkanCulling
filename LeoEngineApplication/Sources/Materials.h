#pragma once

#include "Shaders.h"

#include <vector>
#include <unordered_map>

#include <vulkan/vulkan.h>

class DescriptorAllocator;
class DescriptorLayoutCache;

enum class GraphicsShaderPassType {
	FORWARD,
};

class GraphicsShaderPass {
public:
	struct Parameters {
		VkDevice device = VK_NULL_HANDLE;
		ShaderBuilder* shaderBuilder = nullptr;
		DescriptorAllocator* descriptorAllocator = nullptr;
		DescriptorLayoutCache* descriptorLayoutCache = nullptr;
		std::unordered_map<VkShaderStageFlagBits, const char*> shaderPaths;
	};

	void init(const Parameters& parameters);

private:
	VkDevice _device = VK_NULL_HANDLE;
	DescriptorAllocator* _descriptorAllocator = nullptr;
	DescriptorLayoutCache* _descriptorLayoutCache = nullptr;
	ShaderBuilder* _shaderBuilder = nullptr;

	std::vector<VkDescriptorSet> _descriptorSets;
	std::vector<VkDescriptorSetLayout> _descriptorSetLayouts;
	VkPipeline _pipeline = VK_NULL_HANDLE;
	VkPipelineLayout _pipelineLayout = VK_NULL_HANDLE;

	std::unordered_map<VkShaderStageFlagBits, VkShaderModule> _shaderModules;
};

class MaterialTemplate
{
public:
	struct Parameters {
		VkDevice device = VK_NULL_HANDLE;
		ShaderBuilder* shaderBuilder = nullptr;
		DescriptorAllocator* descriptorAllocator = nullptr;
		DescriptorLayoutCache* descriptorLayoutCache = nullptr;
		std::unordered_map<GraphicsShaderPassType, GraphicsShaderPass::Parameters> passesParameters;
	};

	void init(const Parameters& parameters);

private:
	VkDevice _device;
	DescriptorAllocator* _descriptorAllocator;
	DescriptorLayoutCache* _descriptorLayoutCache;

	std::unordered_map<GraphicsShaderPassType, GraphicsShaderPass> _shaderPasses;
};

