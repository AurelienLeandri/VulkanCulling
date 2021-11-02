#pragma once

#include "vulkan/vulkan.h"

#include <vector>

class ShaderBuilder {
public:
    ShaderBuilder(VkDevice device);

public:
    void createShaderModule(const char* glslFilePath, VkShaderModule& shaderModule, std::vector<char>* buffer = nullptr);

private:
    VkDevice _device;
};

