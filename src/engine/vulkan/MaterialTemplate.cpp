#include "MaterialTemplate.h"

void MaterialTemplate::init(const Parameters& parameters)
{
	_device = parameters.device;
	for (const auto& [passType, passParameters] : parameters.passesParameters) {
		_shaderPasses[passType] = std::make_unique<ShaderPass>();
		_pipelineLayouts[passType] = _shaderPasses[passType]->reflectShaderModules(passParameters);
	}
	_pipelines = parameters.pipelines;
}

void MaterialTemplate::cleanup()
{
	for (auto& [passType, pass] : _shaderPasses) {
		pass->cleanup();
	}
	_shaderPasses.clear();

	for (auto& [stage, pipeline] : _pipelines) {
		vkDestroyPipeline(_device, pipeline, nullptr);
	}

	for (auto& [stage, layout] : _pipelineLayouts) {
		vkDestroyPipelineLayout(_device, layout, nullptr);
	}
	_pipelineLayouts.clear();
}

const ShaderPass* MaterialTemplate::getShaderPass(ShaderPass::Type passType) const
{
	return _shaderPasses.at(passType).get();
}

ShaderPass* MaterialTemplate::getShaderPass(ShaderPass::Type passType)
{
	if (_shaderPasses.find(passType) == _shaderPasses.end()) {
		return nullptr;
	}

	return _shaderPasses[passType].get();
}

void MaterialTemplate::setPipeline(ShaderPass::Type passType, VkPipeline pipeline)
{
	_pipelines[passType] = pipeline;
}

VkPipeline MaterialTemplate::getPipeline(ShaderPass::Type passType) const
{
	return _pipelines.at(passType);
}

VkPipelineLayout MaterialTemplate::getPipelineLayout(ShaderPass::Type passType) const
{
	return _pipelineLayouts.at(passType);
}
