#pragma once

#include <vulkan/vulkan.h>

#include <unordered_map>
#include <vector>

class ShaderBuilder;

class ShaderPass {
public:
	enum class Type {
		FORWARD,
		COMPUTE,
		NB_TYPES
	};

public:
	struct Parameters {
		VkDevice device = VK_NULL_HANDLE;
		ShaderBuilder* shaderBuilder = nullptr;
		std::unordered_map<VkShaderStageFlagBits, const char*> shaderPaths;
		std::unordered_map<std::string, VkDescriptorType> descriptorTypeOverwrites;
	};

	VkPipelineLayout reflectShaderModules(const Parameters& parameters);
	void cleanup();
	void destroyShaderModules();

	const std::unordered_map<VkShaderStageFlagBits, VkShaderModule>& getShaderModules() const;

private:
	VkDevice _device = VK_NULL_HANDLE;
	ShaderBuilder* _shaderBuilder = nullptr;

	std::vector<VkDescriptorSetLayout> _descriptorSetLayouts;

	std::unordered_map<VkShaderStageFlagBits, VkShaderModule> _shaderModules;
};

