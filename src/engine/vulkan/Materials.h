#pragma once

#include "VulkanUtils.h"
#include "DescriptorUtils.h"
#include "ShaderPass.h"

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
	const VkDescriptorSet& getDescriptorSet(ShaderPass::Type type) const;
	VkDescriptorSet& getDescriptorSet(ShaderPass::Type type);
	MaterialType getType() const;

	std::array<MaterialTexture, 5> textures = { {} };

private:
	MaterialBuilder* _builder;
	const MaterialTemplate* _materialTemplate;
	MaterialType _type;
	std::unordered_map<ShaderPass::Type, VkDescriptorSet> _descriptorSets;

};

