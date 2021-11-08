#pragma once

#include "VulkanInstance.h"

#include "DescriptorUtils.h"
#include "Shaders.h"
#include "Materials.h"

#include <memory>
#include <unordered_map>
#include <map>

#include <glm/glm.hpp>

namespace leo {
	class Scene;
	class Material;
	class PerformanceMaterial;
	class Mesh;
	class Shape;
	class Transform;
	class ImageTexture;
}

struct GPUCameraData {
	glm::mat4 view;
	glm::mat4 proj;
	glm::mat4 viewProj;
};

struct GPUSceneData {
	glm::vec4 ambientColor = { 0, 0, 0, 0 };
	glm::vec4 sunlightDirection = { 0, -1, 0, 0 };
	glm::vec4 sunlightColor = { 1, 1, 1, 1 };
};


struct GPUObjectData {
	glm::mat4 modelMatrix;
};

struct AllocatedBuffer {
	VkBuffer buffer = VK_NULL_HANDLE;
	VkDeviceMemory deviceMemory = VK_NULL_HANDLE;
};

struct AllocatedImage {
	VkImage image = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	VkImageView view = VK_NULL_HANDLE;
	VkSampler textureSampler = VK_NULL_HANDLE;
	uint32_t mipLevels = 0;
};

struct FrameData {
	VkSemaphore presentSemaphore = VK_NULL_HANDLE;
	VkSemaphore renderSemaphore = VK_NULL_HANDLE;
	VkFence renderFinishedFence = VK_NULL_HANDLE;

	VkFramebuffer framebuffer = VK_NULL_HANDLE;
	VkCommandPool commandPool = VK_NULL_HANDLE;
	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
};

struct RenderableObject {
	size_t index = 0;
	AllocatedBuffer vertexBuffer = {};
	AllocatedBuffer indexBuffer = {};
	const leo::Shape* sceneShape = nullptr;
	const leo::Material* material = nullptr;
	const leo::Transform* transform = nullptr;
	size_t nbElements = 0;
};

struct ObjectsBatch {
	AllocatedBuffer vertexBuffer = {};
	AllocatedBuffer indexBuffer = {};
	const leo::PerformanceMaterial* material = nullptr;
	const leo::Mesh* mesh = nullptr;
	uint32_t nbObjects = 0;
	uint32_t primitivesPerObject = 0;
	uint32_t stride = 0;
	uint32_t offset = 0;
};

struct GlobalBuffers {
	AllocatedBuffer cameraBuffer = {};
	AllocatedBuffer sceneBuffer = {};
	AllocatedBuffer materialsBuffer = {};
	AllocatedBuffer objectsDataBuffer = {};
};


class VulkanRenderer
{
public:
	struct Options {
	};

	VulkanRenderer(VulkanInstance* vulkan, Options options = {});
	~VulkanRenderer();

public:
	void setScene(const leo::Scene* scene);
	void setCamera(const leo::Camera* camera);
	void init();
	void iterate();

private:
	void _constructSceneRelatedStructures();
	void _createCommandPools();
	void _createInputImages();
	void _createRenderPass();
	void _createGraphicsPipeline();
	void _createFramebufferImageResources();
	void _createFramebuffers();
	void _createCommandBuffers();
	void _createSyncObjects();
	void _updateFrameLevelUniformBuffers(uint32_t currentImage);

	void _createInputBuffers();
	void _createDescriptors();

	void _recreateSwapChainDependentResources();

	void _createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
		VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	void _copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
	void _createGPUBuffer(VkDeviceSize size, VkBufferUsageFlags usage, const void* data, AllocatedBuffer& buffer);
	void _transitionImageLayout(AllocatedImage& imageData, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
	void _copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
	void _generateMipmaps(AllocatedImage& imageData, VkFormat imageFormat, int32_t texWidth, int32_t texHeight);

	VkCommandBuffer _beginSingleTimeCommands(VkCommandPool& commandPool);
	void _endSingleTimeCommands(VkCommandBuffer commandBuffer, VkCommandPool& commandPool);

	void _cleanup();
	void _cleanupSwapChainDependentResources();

private:
	Options _options;

	// Data from other objects
	const leo::Camera* _camera = nullptr;
	const leo::Scene* _scene = nullptr;
	VulkanInstance* _vulkan = nullptr;
	VkDevice _device = VK_NULL_HANDLE;

	// Builders and helpers
	MaterialBuilder _materialBuilder;

	// Pools
	VkCommandPool _mainCommandPool = VK_NULL_HANDLE;

	// Main pipeline
	VkPipelineLayout _pipelineLayout = VK_NULL_HANDLE;
	VkRenderPass _renderPass = VK_NULL_HANDLE;
	VkPipeline _graphicsPipeline = VK_NULL_HANDLE;

	// Per-frame data
	std::vector<FrameData> _framesData;

	// Descriptors shared between frames
	DescriptorAllocator _globalDescriptorAllocator;
	DescriptorLayoutCache _descriptorLayoutCache;
	VkDescriptorSetLayout _globalDataDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorSet _globalDataDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout _objectsDataDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorSet _objectsDataDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout _materialDescriptorSetLayout = VK_NULL_HANDLE;
	std::unordered_map<const leo::Material*, VkDescriptorSet> _materialDescriptorSets;

	// Buffers
	AllocatedBuffer _cameraDataBuffer;
	AllocatedBuffer _sceneDataBuffer;
	AllocatedBuffer _objectsDataBuffer;
	AllocatedBuffer _indirectCommandBuffer;

	// Data shared between framebuffers
	AllocatedImage _framebufferColor;
	VkFormat _depthBufferFormat = VK_FORMAT_UNDEFINED;
	AllocatedImage _framebufferDepth;

	AllocatedImage _testImage;
	VkDescriptorSet _testTextureDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout _testDescriptorSetLayout = VK_NULL_HANDLE;

	// Constant input data
	std::vector<ObjectsBatch> _objectsBatches;

	// Order within each vector: diffuse, specular, ambient, normals, height
	std::unordered_map<const leo::Material*, std::vector<AllocatedImage>> _materialsImages;
	size_t _nbMaterials = 0;

	// Synchronization-related data for the iterate() function.
	static const int _MAX_FRAMES_IN_FLIGHT = 2;
	static const int _MAX_NUMBER_OBJECTS = 10000;
	size_t _currentFrame = 0;

	// Scene data
	std::vector<AllocatedImage> _images;
	std::unordered_map<std::string, AllocatedImage> _sceneTextures;
	void _loadTexture(const leo::ImageTexture& texture);
};

