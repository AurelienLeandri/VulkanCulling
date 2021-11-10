#pragma once

#include "VulkanUtils.h"
#include "DescriptorUtils.h"
#include "GraphicShaderPass.h"

#include <vector>
#include <unordered_map>
#include <memory>

#include <vulkan/vulkan.h>

class DescriptorAllocator;
class DescriptorLayoutCache;
class VulkanInstance;

class MaterialBuilder;
class MaterialTemplate;

struct MaterialTexture {
	VkSampler sampler;
	VkImageView view;
};

enum class MaterialType {
	INVALID = 0,
	BASIC
};

class Material {
public:
	Material(MaterialBuilder* builder, const MaterialTemplate* materialTemplate, MaterialType type);

	const MaterialTemplate* getTemplate() const;
	const VkDescriptorSet& getDescriptorSet(GraphicShaderPass::Type type) const;
	VkDescriptorSet& getDescriptorSet(GraphicShaderPass::Type type);
	MaterialType getType() const;

	std::array<MaterialTexture, 5> textures = { {} };

private:
	MaterialBuilder* _builder;
	const MaterialTemplate* _materialTemplate;
	MaterialType _type;
	std::unordered_map<GraphicShaderPass::Type, VkDescriptorSet> _descriptorSets;

};

