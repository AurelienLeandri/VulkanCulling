#pragma once

#include "GraphicShaderPass.h"

#include <vulkan/vulkan.h>

#include <memory>

class ShaderBuilder;

class MaterialTemplate
{
public:
	struct Parameters {
		VkDevice device = VK_NULL_HANDLE;
		ShaderBuilder* shaderBuilder = nullptr;
		std::unordered_map<ShaderPass::Type, ShaderPass::Parameters> passesParameters;
		std::unordered_map<ShaderPass::Type, VkPipeline> pipelines;
	};

	void init(const Parameters& parameters);
	void cleanup();
	const ShaderPass* getShaderPass(ShaderPass::Type passType) const;
	ShaderPass* getShaderPass(ShaderPass::Type passType);
	void setPipeline(ShaderPass::Type passType, VkPipeline pipeline);
	VkPipeline getPipeline(ShaderPass::Type passType) const;
	VkPipelineLayout getPipelineLayout(ShaderPass::Type passType) const;

private:
	VkDevice _device;

	std::unordered_map<ShaderPass::Type, VkPipeline> _pipelines;
	std::unordered_map<ShaderPass::Type, VkPipelineLayout> _pipelineLayouts;
	std::unordered_map<ShaderPass::Type, std::unique_ptr<ShaderPass>> _shaderPasses;
};

