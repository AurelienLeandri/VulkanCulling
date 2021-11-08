#pragma once

#include "Shaders.h"
#include "VulkanUtils.h"
#include "DescriptorUtils.h"

#include <vector>
#include <unordered_map>
#include <memory>

#include <vulkan/vulkan.h>

class DescriptorAllocator;
class DescriptorLayoutCache;
class VulkanInstance;

enum class GraphicsShaderPassType {
	FORWARD,
	NB_TYPES
};

class GraphicsShaderPass {
public:
	struct Parameters {
		VkDevice device = VK_NULL_HANDLE;
		ShaderBuilder* shaderBuilder = nullptr;
		std::unordered_map<VkShaderStageFlagBits, const char*> shaderPaths;
		std::unordered_map<std::string, VkDescriptorType> descriptorTypeOverwrites;
	};

	void init(const Parameters& parameters);
	void setPipeline(VkPipeline pipeline);
	const VkPipeline getPipeline() const;
	VkPipelineLayout getPipelineLayout() const;

	const std::unordered_map<VkShaderStageFlagBits, VkShaderModule>& getShaderModules() const;

private:
	VkDevice _device = VK_NULL_HANDLE;
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
		std::unordered_map<GraphicsShaderPassType, GraphicsShaderPass::Parameters> passesParameters;
	};

	void init(const Parameters& parameters);
	const GraphicsShaderPass* getShaderPass(GraphicsShaderPassType passType) const;
	GraphicsShaderPass* getShaderPass(GraphicsShaderPassType passType);

private:
	VkDevice _device;

	std::unordered_map<GraphicsShaderPassType, std::unique_ptr<GraphicsShaderPass>> _shaderPasses;
};

struct MaterialTexture {
	VkSampler sampler;
	VkImageView view;
};

class MaterialBuilder;

enum class MaterialType {
	INVALID = 0,
	BASIC
};

class Material {
public:
	Material(MaterialBuilder* builder, const MaterialTemplate* materialTemplate);

	void initDescriptorSet();
	const MaterialTemplate* getTemplate() const;

public:
	std::array<MaterialTexture, 5> textures = { {} };
	std::unordered_map<GraphicsShaderPassType, VkDescriptorSet> descriptorSets;

private:
	MaterialBuilder* _builder;
	const MaterialTemplate* _materialTemplate;

};

class MaterialBuilder {
public:
	struct Parameters {
		VkSampleCountFlagBits multisamplingNbSamples = VK_SAMPLE_COUNT_1_BIT;
		VkRenderPass forwardRenderPass = VK_NULL_HANDLE;
	};

public:
	MaterialBuilder(VkDevice device, const VulkanInstance* instance);

	void init(Parameters parameters = {});
	Material* createMaterial(MaterialType type);
	void setupMaterialDescriptorSet(Material& material);

private:
	const VulkanInstance* _vulkan;
	PipelineBuilder _forwardPipelineBuilder;
	Parameters _parameters;
	VkDevice _device;
	MaterialTemplate _forwardPassTemplate;
	ShaderBuilder _shaderBuilder;
	DescriptorAllocator _descriptorAllocator;
	DescriptorLayoutCache _descriptorLayoutCache;
	std::vector<std::unique_ptr<Material>> _materials;
};

