#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include <vector>
#include <array>

class VulkanUtils
{

/*
* Initializers
*/

public:
	static VkCommandPoolCreateInfo createCommandPoolInfo(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0);
	static VkCommandBufferAllocateInfo createCommandBufferAllocateInfo(VkCommandPool commandPool, uint32_t nbCommandBuffers, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	static VkPipelineDepthStencilStateCreateInfo createDepthStencilCreateInfo(bool depthTest, bool depthWrite, VkCompareOp compareOp);
};

