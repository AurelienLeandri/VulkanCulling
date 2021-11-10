#pragma once

#include <vulkan/vulkan.h>

#include <unordered_map>
#include <vector>

class ShaderBuilder;

class GraphicShaderPass {
public:
	enum class Type {
		FORWARD,
		NB_TYPES
	};

public:
	struct Parameters {
		VkDevice device = VK_NULL_HANDLE;
		ShaderBuilder* shaderBuilder = nullptr;
		std::unordered_map<VkShaderStageFlagBits, const char*> shaderPaths;
		std::unordered_map<std::string, VkDescriptorType> descriptorTypeOverwrites;
	};

	void init(const Parameters& parameters);
	void cleanup();
	void setPipeline(VkPipeline pipeline);
	const VkPipeline getPipeline() const;
	VkPipelineLayout getPipelineLayout() const;

	const std::unordered_map<VkShaderStageFlagBits, VkShaderModule>& getShaderModules() const;

private:
	VkDevice _device = VK_NULL_HANDLE;
	ShaderBuilder* _shaderBuilder = nullptr;

	std::vector<VkDescriptorSetLayout> _descriptorSetLayouts;
	VkPipeline _pipeline = VK_NULL_HANDLE;
	VkPipelineLayout _pipelineLayout = VK_NULL_HANDLE;

	std::unordered_map<VkShaderStageFlagBits, VkShaderModule> _shaderModules;
};

