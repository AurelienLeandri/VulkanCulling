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
		std::unordered_map<GraphicShaderPass::Type, GraphicShaderPass::Parameters> passesParameters;
	};

	void init(const Parameters& parameters);
	void cleanup();
	const GraphicShaderPass* getShaderPass(GraphicShaderPass::Type passType) const;
	GraphicShaderPass* getShaderPass(GraphicShaderPass::Type passType);

private:
	VkDevice _device;

	std::unordered_map<GraphicShaderPass::Type, std::unique_ptr<GraphicShaderPass>> _shaderPasses;
};

