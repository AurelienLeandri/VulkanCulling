#pragma once

#include "vulkan/vulkan.h"

#include <vector>

class ShaderBuilder {
public:
    void init(VkDevice device);
    void createShaderModule(const char* glslFilePath, VkShaderModule& shaderModule, std::vector<char>* buffer = nullptr);

private:
    VkDevice _device = VK_NULL_HANDLE;
};

