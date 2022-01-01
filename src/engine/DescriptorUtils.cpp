#include "DescriptorUtils.h"

#include <algorithm>

DescriptorAllocator::DescriptorAllocator(VkDevice device) :
	_device(device)
{
}

void DescriptorAllocator::init(const Options& options)
{
	_options = options;
	if (!_options.poolSizes.size()) {
		_options.poolSizes = {
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 0.5f },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4.f },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4.f },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.f },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1.f },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1.f },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2.f },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2.f },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1.f },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1.f },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 0.5f }
		};
	}
}

int DescriptorAllocator::allocate(VkDescriptorSet& set, VkDescriptorSetLayout layout)
{
	if (!_currentPool) {  // First time or after reset: the _currentPool handle is null
		_currentPool = _getPool();
		_poolsInUse.push_back(_currentPool);
	}

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;

	allocInfo.pSetLayouts = &layout;
	allocInfo.descriptorPool = _currentPool;
	allocInfo.descriptorSetCount = 1;

	VkResult allocationResult = vkAllocateDescriptorSets(_device, &allocInfo, &set);
	switch (allocationResult) {
	case VK_SUCCESS:
		return 0;
	case VK_ERROR_FRAGMENTED_POOL:
	case VK_ERROR_OUT_OF_POOL_MEMORY:
		// Need to allocate from another pool
		allocInfo.descriptorPool = _currentPool = _getPool();
		_poolsInUse.push_back(_currentPool);
		return vkAllocateDescriptorSets(_device, &allocInfo, &set) ? -1 : 0;  // WTF if that fails
	default:
		return -1;
	}
}


void DescriptorAllocator::resetAllPools()
{
	for (VkDescriptorPool& p : _poolsInUse) {
		vkResetDescriptorPool(_device, p, 0);
		_availablePools.push_back(p);
	}
	_poolsInUse.clear();

	_currentPool = VK_NULL_HANDLE;
}

VkDescriptorPool DescriptorAllocator::_getPool()
{
	if (_availablePools.size()) {
		VkDescriptorPool pool = _availablePools.back();
		_availablePools.pop_back();
		return pool;
	}
	else {
		return _createPool();
	}
}

void DescriptorAllocator::cleanup()
{
	for (VkDescriptorPool& pool : _poolsInUse) {
		vkDestroyDescriptorPool(_device, pool, nullptr);
	}
	_poolsInUse.clear();

	for (VkDescriptorPool& pool : _availablePools) {
		vkDestroyDescriptorPool(_device, pool, nullptr);
	}
	_availablePools.clear();

	_currentPool = VK_NULL_HANDLE;
}

VkDescriptorPool DescriptorAllocator::_createPool()
{
	std::vector<VkDescriptorPoolSize> sizes;
	sizes.reserve(_options.poolSizes.size());
	for (auto& entry : _options.poolSizes) {
		sizes.push_back({ entry.first, static_cast<uint32_t>(entry.second) * _options.poolBaseSize });
	}

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = _options.flags;
	pool_info.maxSets = _options.poolBaseSize;
	pool_info.poolSizeCount = static_cast<uint32_t>(sizes.size());
	pool_info.pPoolSizes = sizes.data();

	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	vkCreateDescriptorPool(_device, &pool_info, nullptr, &descriptorPool);

	return descriptorPool;
}

DescriptorLayoutCache::DescriptorLayoutCache(VkDevice device) :
	_device(device)
{
}

void DescriptorLayoutCache::cleanup()
{
	for (auto& entry : _cache) {
		vkDestroyDescriptorSetLayout(_device, entry.second, nullptr);
	}

	_cache.clear();
}

VkDescriptorSetLayout DescriptorLayoutCache::createDescriptorLayout(const VkDescriptorSetLayoutCreateInfo& info)
{
	DescriptorLayoutInfo key{ {info.pBindings, info.pBindings + info.bindingCount} };
	std::sort(key.bindings.begin(), key.bindings.end(), [](VkDescriptorSetLayoutBinding& a, VkDescriptorSetLayoutBinding& b) {
		return a.binding < b.binding;
		});
	if (_cache.find(key) == _cache.end()) {
		VkDescriptorSetLayout newLayout = VK_NULL_HANDLE;
		if (vkCreateDescriptorSetLayout(_device, &info, nullptr, &newLayout)) {
			return VK_NULL_HANDLE;
		}
		_cache[key] = newLayout;
	}
	return _cache[key];
}

std::size_t DescriptorLayoutCache::_DescriptorLayoutHash::operator()(const DescriptorLayoutInfo& k) const
{
	return k.hash();
}

bool DescriptorLayoutCache::DescriptorLayoutInfo::operator==(const DescriptorLayoutInfo& other) const
{
	// Both vectors are sorted at this point
	if (bindings.size() != other.bindings.size())
		return false;

	for (size_t i = 0; i < bindings.size(); ++i) {
		if (bindings[i].binding != other.bindings[i].binding ||
			bindings[i].descriptorCount != other.bindings[i].descriptorCount ||
			bindings[i].descriptorType != other.bindings[i].descriptorType ||
			bindings[i].stageFlags != other.bindings[i].stageFlags)
		{
			return false;
		}
	}

	return true;
}

size_t DescriptorLayoutCache::DescriptorLayoutInfo::hash() const
{
	std::size_t result = std::hash<std::size_t>()(bindings.size());

	for (const VkDescriptorSetLayoutBinding& b : bindings)
	{
		// Pack the binding data into a single int64. Not fully correct but it's ok
		std::size_t binding_hash = b.binding | b.descriptorType << 8 | b.descriptorCount << 16 | b.stageFlags << 24;

		// Shuffle the packed binding data and xor it with the main hash
		result ^= std::hash<std::size_t>()(binding_hash);
	}

	return result;
}

DescriptorBuilder DescriptorBuilder::begin(VkDevice device, DescriptorLayoutCache& layoutCache, DescriptorAllocator& allocator)
{
	return DescriptorBuilder(device, layoutCache, allocator);
}

DescriptorBuilder& DescriptorBuilder::bindBuffer(uint32_t binding, VkDescriptorBufferInfo& bufferInfo, VkDescriptorType type, VkShaderStageFlags stageFlags)
{
	VkDescriptorSetLayoutBinding newBinding = {};

	newBinding.descriptorCount = 1;
	newBinding.descriptorType = type;
	newBinding.pImmutableSamplers = nullptr;
	newBinding.stageFlags = stageFlags;
	newBinding.binding = binding;

	_bindings.push_back(newBinding);

	VkWriteDescriptorSet newWrite = {};
	newWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	newWrite.pNext = nullptr;

	newWrite.descriptorCount = 1;
	newWrite.descriptorType = type;
	newWrite.pBufferInfo = &bufferInfo;
	newWrite.dstBinding = binding;

	_writes.push_back(newWrite);
	return *this;
}

DescriptorBuilder& DescriptorBuilder::bindImage(uint32_t binding, VkDescriptorImageInfo& imageInfo, VkDescriptorType type, VkShaderStageFlags stageFlags)
{
	VkDescriptorSetLayoutBinding newBinding = {};

	newBinding.descriptorCount = 1;
	newBinding.descriptorType = type;
	newBinding.pImmutableSamplers = nullptr;
	newBinding.stageFlags = stageFlags;
	newBinding.binding = binding;

	_bindings.push_back(newBinding);

	VkWriteDescriptorSet newWrite = {};
	newWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	newWrite.pNext = nullptr;

	newWrite.descriptorCount = 1;
	newWrite.descriptorType = type;
	newWrite.pImageInfo = &imageInfo;
	newWrite.dstBinding = binding;

	_writes.push_back(newWrite);
	return *this;
}

bool DescriptorBuilder::build(VkDescriptorSet& set, VkDescriptorSetLayout& layout)
{
	//build layout first
	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.pNext = nullptr;

	layoutInfo.pBindings = _bindings.data();
	layoutInfo.bindingCount = static_cast<uint32_t>(_bindings.size());

	layout = _cache.createDescriptorLayout(layoutInfo);

	//allocate descriptor
	if (_allocator.allocate(set, layout)) { return false; };

	//write descriptor
	for (VkWriteDescriptorSet& w : _writes) {
		w.dstSet = set;
	}

	vkUpdateDescriptorSets(_device, static_cast<uint32_t>(_writes.size()), _writes.data(), 0, nullptr);

	return true;
}

bool DescriptorBuilder::build(VkDescriptorSet& set)
{
	//build layout first
	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.pNext = nullptr;

	layoutInfo.pBindings = _bindings.data();
	layoutInfo.bindingCount = static_cast<uint32_t>(_bindings.size());

	//allocate descriptor
	if (_allocator.allocate(set, _cache.createDescriptorLayout(layoutInfo))) { return false; }

	//write descriptor
	for (VkWriteDescriptorSet& w : _writes) {
		w.dstSet = set;
	}

	vkUpdateDescriptorSets(_device, static_cast<uint32_t>(_writes.size()), _writes.data(), 0, nullptr);

	return true;
}

DescriptorBuilder::DescriptorBuilder(VkDevice device, DescriptorLayoutCache& layoutCache, DescriptorAllocator& allocator)
	: _device(device), _cache(layoutCache), _allocator(allocator)
{
}
