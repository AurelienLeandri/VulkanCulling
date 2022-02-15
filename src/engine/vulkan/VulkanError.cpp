#include "VulkanError.h"

#include <sstream>

VulkanRendererException::VulkanRendererException(VkResult error, const char* message) :
	_error(error), _message(message)
{
}

VulkanRendererException::VulkanRendererException(const char* message) :
	_error(VK_SUCCESS), _message(message)
{
}

const char* VulkanRendererException::what() const noexcept
{
	return _message;
}

void VK_CHECK(VkResult err)
{
	if (err)
	{
		throw VulkanRendererException(err, nullptr);
	}
}
