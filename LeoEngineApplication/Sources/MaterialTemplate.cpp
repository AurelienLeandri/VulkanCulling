#include "MaterialTemplate.h"

void MaterialTemplate::init(const Parameters& parameters)
{
	_device = parameters.device;
	for (const auto& [passType, passParameters] : parameters.passesParameters) {
		_shaderPasses[passType] = std::make_unique<GraphicShaderPass>();
		_shaderPasses[passType]->init(passParameters);

	}
}

void MaterialTemplate::cleanup()
{
	for (auto& [passType, pass] : _shaderPasses) {
		pass->cleanup();
	}
	_shaderPasses.clear();
}

const GraphicShaderPass* MaterialTemplate::getShaderPass(GraphicShaderPass::Type passType) const
{
	return _shaderPasses.at(passType).get();
}

GraphicShaderPass* MaterialTemplate::getShaderPass(GraphicShaderPass::Type passType)
{
	if (_shaderPasses.find(passType) == _shaderPasses.end()) {
		return nullptr;
	}

	return _shaderPasses[passType].get();
}
