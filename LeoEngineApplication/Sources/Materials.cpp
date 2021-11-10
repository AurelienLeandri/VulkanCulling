#include "Materials.h"

#include "MaterialBuilder.h"

Material::Material(MaterialBuilder* builder, const MaterialTemplate* materialTemplate, MaterialType type) :
	_builder(builder), _materialTemplate(materialTemplate), _type(type)
{
}

const MaterialTemplate* Material::getTemplate() const
{
	return _materialTemplate;
}

const VkDescriptorSet& Material::getDescriptorSet(GraphicShaderPass::Type type) const
{
	return _descriptorSets.at(type);
}

VkDescriptorSet& Material::getDescriptorSet(GraphicShaderPass::Type type)
{
	return _descriptorSets[type];
}

MaterialType Material::getType() const
{
	return _type;
}
