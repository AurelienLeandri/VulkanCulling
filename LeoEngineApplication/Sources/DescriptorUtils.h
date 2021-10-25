#pragma once

#include <vulkan/vulkan.h>

#include <vector>
#include <unordered_map>

class DescriptorAllocator
{
public:

	struct Options {
		std::unordered_map<VkDescriptorType, float> poolSizes;
		VkDescriptorPoolCreateFlags flags = 0;
		uint32_t poolBaseSize = 1000;
	};

public:
	DescriptorAllocator(VkDevice device);
	void init(const Options& options = {});
	void cleanup();

	void resetAllPools();
	int allocate(VkDescriptorSet& set, VkDescriptorSetLayout layout);

private:
	VkDescriptorPool _getPool();
	VkDescriptorPool _createPool();

private:
	VkDevice _device = VK_NULL_HANDLE;
	std::vector<VkDescriptorPool> _poolsInUse;
	std::vector<VkDescriptorPool> _availablePools;
	VkDescriptorPool _currentPool = VK_NULL_HANDLE;
	Options _options;
};

class DescriptorLayoutCache {
public:
	DescriptorLayoutCache(VkDevice device);
	void cleanup();

	VkDescriptorSetLayout createDescriptorLayout(const VkDescriptorSetLayoutCreateInfo& info);

	struct DescriptorLayoutInfo {
		std::vector<VkDescriptorSetLayoutBinding> bindings;

		// Necessary for hashing
		bool operator==(const DescriptorLayoutInfo& other) const;
		size_t hash() const;
	};


private:
	struct _DescriptorLayoutHash {
		std::size_t operator()(const DescriptorLayoutInfo& k) const;
	};

	std::unordered_map<DescriptorLayoutInfo, VkDescriptorSetLayout, _DescriptorLayoutHash> _cache;
	VkDevice _device;
};

class DescriptorBuilder {
public:
	static DescriptorBuilder begin(VkDevice device, DescriptorLayoutCache& layoutCache, DescriptorAllocator& allocator);

	DescriptorBuilder& bindBuffer(uint32_t binding, VkDescriptorBufferInfo& bufferInfo, VkDescriptorType type, VkShaderStageFlags stageFlags);
	DescriptorBuilder& bindImage(uint32_t binding, VkDescriptorImageInfo& imageInfo, VkDescriptorType type, VkShaderStageFlags stageFlags);

	bool build(VkDescriptorSet& set, VkDescriptorSetLayout& layout);
	bool build(VkDescriptorSet& set);

private:
	DescriptorBuilder(VkDevice device, DescriptorLayoutCache& layoutCache, DescriptorAllocator& allocator);

	std::vector<VkWriteDescriptorSet> _writes;
	std::vector<VkDescriptorSetLayoutBinding> _bindings;
	DescriptorLayoutCache& _cache;
	DescriptorAllocator& _allocator;
	VkDevice _device = VK_NULL_HANDLE;
};



