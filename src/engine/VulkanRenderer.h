#pragma once

#include "VulkanInstance.h"

#include "DescriptorUtils.h"
#include "MaterialBuilder.h"

#include <memory>
#include <unordered_map>
#include <map>

#include <scene/GeometryIncludes.h>

namespace leoscene {
	class Scene;
	class Material;
	class PerformanceMaterial;
	class Mesh;
	class Shape;
	class Transform;
	class ImageTexture;
}

/*
* Camera transform matrices
*/
struct GPUCameraData {
	glm::mat4 view;
	glm::mat4 proj;
	glm::mat4 viewProj;
	glm::mat4 invProj;
};

/*
* Global scene data. Not used at the moment.
*/
struct GPUSceneData {
	glm::vec4 ambientColor = { 0, 0, 0, 0 };
	glm::vec4 sunlightDirection = { 0, -1, 0, 0 };
	glm::vec4 sunlightColor = { 1, 1, 1, 1 };
};

/*
* For an instance of a mesh, stores the batch in witch the instance is located and the index of the instance's data (see GPUObjectData)
*/
struct GPUObjectInstance {
	uint32_t batchId = 0;
	uint32_t dataId = 0;
};

/*
* Stores the draw command parameters for a batch
*/
struct GPUIndirectDrawCommand {
	VkDrawIndexedIndirectCommand command = {};
};

/*
* Global data used for culling compute shaders
*/
struct GPUCullingGlobalData {
	glm::mat4 viewMatrix = glm::mat4(1);
	glm::vec4 frustum[6] = { glm::vec4(0) };
	float zNear = 0;
	float zFar = 10000.f;
	float P00 = 0;
	float P11 = 0;
	int pyramidWidth = 0;
	int pyramidHeight = 0;
	uint32_t nbInstances = 0;
	bool cullingEnabled = false;
};

/*
* Data relative to each object instance. Instances will differ only by these data.
*/
struct GPUObjectData {
	glm::mat4 modelMatrix;
	glm::vec4 sphereBounds;
};

struct ShapeData {
	AllocatedBuffer vertexBuffer;
	AllocatedBuffer indexBuffer;
	uint32_t nbElements = 0;
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
	const leoscene::Shape* sceneShape = nullptr;
	const leoscene::Material* material = nullptr;
	const leoscene::Transform* transform = nullptr;
	size_t nbElements = 0;
};

struct DrawCallInfo {
	const Material* material = nullptr;
	const ShapeData* shape = nullptr;
	uint32_t nbObjects = 0;
	uint32_t primitivesPerObject = 0;
};

class VulkanRenderer
{
public:
	struct Options {
	};

	VulkanRenderer(VulkanInstance* vulkan, Options options = {});
	~VulkanRenderer();

public:
	void setCamera(const leoscene::Camera* camera);
	void init();
	void loadSceneToDevice(const leoscene::Scene* scene);
	void iterate();

private:
	void _updateCamera(uint32_t currentImage);

	void _fillConstantGlobalBuffers(const leoscene::Scene* scene);
	void _createComputePipeline(const char* shaderPath, VkPipeline& pipeline, VkPipelineLayout& layout, ShaderPass& shaderPass);
	void _createCullingDescriptors(uint32_t nbObjects);
	void _computeDepthPyramid(VkCommandBuffer commandBuffer);
	void _createGlobalDescriptors(uint32_t nbObjects);

	void _cleanup();

private:
	Options _options;

	// Data from other objects
	const leoscene::Camera* _camera = nullptr;
	VulkanInstance* _vulkan = nullptr;
	VkDevice _device = VK_NULL_HANDLE;

	// Builders and helpers
	MaterialBuilder _materialBuilder;
	ShaderBuilder _shaderBuilder;

	// Pools
	VkCommandPool _mainCommandPool = VK_NULL_HANDLE;

	// Main pipeline
	VkRenderPass _renderPass = VK_NULL_HANDLE;

	// Per-frame data
	std::vector<FrameData> _framesData;

	// Descriptors shared between frames
	DescriptorAllocator _globalDescriptorAllocator;
	DescriptorLayoutCache _globalDescriptorLayoutCache;
	VkDescriptorSetLayout _globalDataDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorSet _globalDataDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout _objectsDataDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorSet _objectsDataDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout _materialDescriptorSetLayout = VK_NULL_HANDLE;
	std::unordered_map<const leoscene::Material*, VkDescriptorSet> _materialDescriptorSets;

	// Buffers
	AllocatedBuffer _cameraDataBuffer;
	AllocatedBuffer _sceneDataBuffer;
	AllocatedBuffer _objectsDataBuffer;

	// Data shared between framebuffers
	AllocatedImage _framebufferColor;
	AllocatedImage _framebufferDepth;
	AllocatedImage _depthImage;
	VkSampler _depthImageSampler = VK_NULL_HANDLE;

	// Synchronization-related data for the iterate() function.
	static const int _MAX_FRAMES_IN_FLIGHT = 2;
	static const int _MAX_NUMBER_OBJECTS = 1000000;
	size_t _currentFrame = 0;

	// Scene data
	std::vector<std::unique_ptr<AllocatedImage>> _materialImagesData;
	std::vector<VkSampler> _materialImagesSamplers;
	std::vector<std::unique_ptr<ShapeData>> _shapeData;

	// Indexes and utility containers to group similar objects in the scene
	std::vector<DrawCallInfo> _drawCalls;
	size_t _nbMaterials = 0;

	VkPipeline _cullingPipeline = VK_NULL_HANDLE;
	VkPipelineLayout _cullingPipelineLayout = VK_NULL_HANDLE;
	ShaderPass _cullShaderPass;
	VkPipeline _depthPyramidPipeline = VK_NULL_HANDLE;
	VkPipelineLayout _depthPyramidPipelineLayout = VK_NULL_HANDLE;
	ShaderPass _depthPyramidShaderPass;

	// Culling compute pipeline data
	glm::mat4 _projectionMatrix = glm::mat4(1);
	glm::mat4 _invProjectionMatrix = glm::mat4(1);
	uint32_t _nbInstances = 0;
	DescriptorAllocator _cullingDescriptorAllocator;
	AllocatedBuffer _gpuObjectInstances = {};
	AllocatedBuffer _gpuBatches = {};
	AllocatedBuffer _gpuCullingGlobalData = {};
	VkBufferMemoryBarrier _gpuBatchesBarrier = {};
	AllocatedBuffer _gpuResetBatches = {};
	VkBufferMemoryBarrier _gpuBatchesResetBarrier = {};
	AllocatedBuffer _gpuIndexToObjectId = {};
	VkBufferMemoryBarrier _gpuIndexToObjectIdBarrier = {};
	VkDescriptorSet _cullingDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout _cullingDescriptorSetLayout = VK_NULL_HANDLE;

	// Depth pyramid computing data
	AllocatedImage _depthPyramid = {};
	uint32_t _depthPyramidWidth = 0;
	uint32_t _depthPyramidHeight = 0;
	std::vector<VkImageView> _depthPyramidLevelViews;
	std::vector<VkDescriptorSet> _depthPyramidDescriptorSets;
	VkDescriptorSetLayout _depthPyramidDescriptorSetLayout = VK_NULL_HANDLE;
	DescriptorAllocator _depthPyramidDescriptorAllocator;
	std::vector<VkImageMemoryBarrier> _depthPyramidMipLevelBarriers;
	VkImageMemoryBarrier _framebufferDepthWriteBarrier = {};
	VkImageMemoryBarrier _framebufferDepthReadBarrier = {};

	bool _sceneLoaded = false;

	// Other
	float _zNear = 0.1f;
	float _zFar = 300.f;
};

