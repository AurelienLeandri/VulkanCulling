#pragma once

#include <exception>

#include <vulkan/vulkan.h>

class VulkanRendererException : public std::exception {
public:
	VulkanRendererException(VkResult error, const char* message = nullptr);
	VulkanRendererException(const char* message);
	virtual const char* what() const noexcept;
private:
	VkResult _error;
	const char* _message = nullptr;
};

void VK_CHECK(VkResult err);

