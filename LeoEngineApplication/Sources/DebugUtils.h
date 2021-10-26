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

#define VK_CHECK(x)                                                 \
	do                                                              \
	{                                                               \
		VkResult err = x;                                           \
		if (err)                                                    \
		{                                                           \
			throw VulkanRendererException(err, nullptr);			\
		}                                                           \
	} while (0)

