#include "ShaderBuilder.h"

#include "DebugUtils.h"

#include <fstream>

ShaderBuilder::ShaderBuilder(VkDevice device)
    : _device(device)
{
}

void ShaderBuilder::createShaderModule(const char* glslFilePath, VkShaderModule& shaderModule, std::vector<char>* buffer)
{
    std::ifstream file(glslFilePath, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw VulkanRendererException((std::string("Failed to open spir-v file \"") + glslFilePath + "\".").c_str());
    }

    std::vector<char> localBuffer;
    if (!buffer) {
        buffer = &localBuffer;
    }
    buffer->resize((size_t)file.tellg());

    file.seekg(0);
    file.read(buffer->data(), buffer->size());

    file.close();

    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = buffer->size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(buffer->data());

    VK_CHECK(vkCreateShaderModule(_device, &createInfo, nullptr, &shaderModule));
}
