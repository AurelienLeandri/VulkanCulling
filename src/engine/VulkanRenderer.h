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
* Dynamic data that can be changed every frame (except the camera). The main camera is in GPUCameraData.
*/
struct GPUDynamicData {
	glm::mat4 cullingViewMatrix;
	glm::vec4 forcedColoring;
	int frustumCulling;
	int occlusionCulling;
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
	glm::vec4 frustum[6] = { glm::vec4(0) };
	float zNear = 0;
	float zFar = 10000.f;
	float P00 = 0;
	float P11 = 0;
	int pyramidWidth = 0;
	int pyramidHeight = 0;
	uint32_t nbInstances = 0;
};

/*
* Data relative to each object instance. Instances will differ only by these data.
*/
struct GPUObjectData {
	glm::mat4 modelMatrix;
	glm::vec4 sphereBounds;
};

/*
* Buffers for each mesh
*/
struct ShapeData {
	AllocatedBuffer vertexBuffer;
	AllocatedBuffer indexBuffer;
	uint32_t nbElements = 0;
};

/*
* Data tied to a frame
*/
struct FrameData {
	VkSemaphore presentSemaphore = VK_NULL_HANDLE;
	VkSemaphore renderSemaphore = VK_NULL_HANDLE;
	VkFence renderFinishedFence = VK_NULL_HANDLE;

	VkFramebuffer framebuffer = VK_NULL_HANDLE;
	VkCommandPool commandPool = VK_NULL_HANDLE;
	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
};

// Data of each separate draw call (mainly the vertex/index buffers and the material to bind)
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

public:
	void setCamera(const leoscene::Camera* camera);
	void init(const ApplicationState* applicationState);
	void loadSceneToDevice(const leoscene::Scene* scene);
	void drawFrame();
	void cleanup();
	void cleanupSwapChainDependentObjects();
	void recreateSwapChainDependentObjects();

private:
	void _updateDynamicData();
	void _drawObjectsCommands(VkCommandBuffer cmd, VkFramebuffer framebuffer);
	void _createMainRenderPass();
	void _fillConstantGlobalBuffers(const leoscene::Scene* scene);
	void _createComputePipeline(const char* shaderPath, VkPipeline& pipeline, VkPipelineLayout& layout, ShaderPass& shaderPass);
	void _createCullingDescriptors(uint32_t nbObjects);
	void _createDepthPyramidDescriptors();
	void _createFramebuffers();
	void _createDepthPyramid();
	void _createDepthSampler();
	void _createBarriers();
	void _computeDepthPyramid(VkCommandBuffer commandBuffer);
	void _createGlobalDescriptors(uint32_t nbObjects);

private:
	// Some options to the renderer. Reserved for later.
	Options _options;

	// Data owned by other objects referenced here for easy access.
	const leoscene::Camera* _camera = nullptr;  // Application camera
	const ApplicationState* _applicationState = nullptr;  // Application state
	VulkanInstance* _vulkan = nullptr;  // Vulkan instance (constains swap chain, VkInstance and general properties)
	VkDevice _device = VK_NULL_HANDLE;  // Logical device owned by the VulkanInstance

	// Builders and helpers
	MaterialBuilder _materialBuilder;
	ShaderBuilder _shaderBuilder;

	// Command pool used mainly for single time transfer operations. Each frame has its own command pool.
	VkCommandPool _mainCommandPool = VK_NULL_HANDLE;

	// Render pass and its attachments
	VkRenderPass _renderPass = VK_NULL_HANDLE;
	AllocatedImage _framebufferColor;  // Multisampled color attachment
	AllocatedImage _framebufferDepth;  // Multisampled depth attachment
	AllocatedImage _depthImage;  // Singlesampled depth resolve attachment
	VkSampler _depthImageSampler = VK_NULL_HANDLE;  // This sampler is used when we need to sample the depth buffer (ex. computing the depth pyramid)
	VkFormat _depthBufferFormat = VK_FORMAT_UNDEFINED;

	// Per-frame data
	std::vector<FrameData> _framesData;

	// Just a flag to check if the scene was loaded
	bool _sceneLoaded = false;

	/*
	* Data for the graphics pipeline
	*/

	// Allocator to manage global descriptors and their layouts.
	DescriptorAllocator _globalDescriptorAllocator;
	DescriptorLayoutCache _globalDescriptorLayoutCache;

	// Global data potentially used by any stage. Meant for update so most buffers pointed at are CPU_TO_GPU flagged.
	// Camera, global scene data (lighting, reserved for later), index map (instance id to transform matrix data)
	// Also a misc buffer containing the application state and some debug data.
	VkDescriptorSetLayout _globalDataDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorSet _globalDataDescriptorSet = VK_NULL_HANDLE;
	AllocatedBuffer _cameraDataBuffer;
	AllocatedBuffer _sceneDataBuffer;
	AllocatedBuffer _objectsDataBuffer;
	AllocatedBuffer _miscDynamicDataBuffer;

	// Constant buffer, allocated and filled when calling loadSceneFromDevice
	// Contains the sphere bounds and the matrix transforms of all object instances.
	VkDescriptorSetLayout _objectsDataDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorSet _objectsDataDescriptorSet = VK_NULL_HANDLE;

	// Materials data.
	VkDescriptorSetLayout _materialDescriptorSetLayout = VK_NULL_HANDLE;
	std::unordered_map<const leoscene::Material*, VkDescriptorSet> _materialDescriptorSets;
	std::vector<std::unique_ptr<AllocatedImage>> _materialImagesData;
	std::vector<VkSampler> _materialImagesSamplers;

	// Some data needed for the drawFrame function.
	static const int _MAX_FRAMES_IN_FLIGHT = 2;
	size_t _currentFrame = 0;

	// Vertex and index buffers
	std::vector<std::unique_ptr<ShapeData>> _shapeData;

	uint32_t _totalInstancesNb = 0;
	size_t _nbMaterials = 0;
	uint32_t _nbInstances = 0;

	// Data related to each draw call (material, instance number etc.)
	std::vector<DrawCallInfo> _drawCalls;

	/*
	* Data for indirect compute based culling
	*/

	// Culling compute pipeline (frustum and occlusion culling)
	VkPipeline _cullingPipeline = VK_NULL_HANDLE;
	VkPipelineLayout _cullingPipelineLayout = VK_NULL_HANDLE;
	ShaderPass _cullShaderPass;

	// Data for used by the culling compute pipeline
	DescriptorAllocator _cullingDescriptorAllocator;
	VkDescriptorSetLayout _cullingDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorSet _cullingDescriptorSet = VK_NULL_HANDLE;
	glm::mat4 _cullingViewMatrix = glm::mat4(1);  // All culling happens from this view. Usually set to be the camera's view matrix, but can be (un)locked by presing "L".
	// Projection matrix of the camera.
	glm::mat4 _projectionMatrix = glm::mat4(1);
	glm::mat4 _invProjectionMatrix = glm::mat4(1);
	float _zNear = 0.1f;
	float _zFar = 300.f;

	AllocatedBuffer _gpuObjectInstances = {};  // For each instance, the batch it belongs to and an index to retrieve the instance's data (transform matrix, bounds)
	AllocatedBuffer _gpuBatches = {};  // Set by the culling shader. For each draw call, the corresponding indirect draw command
	AllocatedBuffer _gpuCullingGlobalData = {};  // Global data used by the culling algorithms: The frustum's representation, among other things.
	AllocatedBuffer _gpuResetBatches = {};  // Constant buffer used to reset the batches buffer each frame.
	AllocatedBuffer _gpuIndexToObjectId = {};  // A map from instance index to the instance's data. Set by the culling shader.

	// Barriers to synchronize access of resources written by the culling algorithm and then read by the render pass.
	VkBufferMemoryBarrier _gpuBatchesBarrier = {};
	VkBufferMemoryBarrier _gpuBatchesResetBarrier = {};
	VkBufferMemoryBarrier _gpuIndexToObjectIdBarrier = {};

	/*
	* Data for computing the depth pyramid used by compute based culling
	*/

	// Compute pipeline to make the depth pyramid from the depth buffer (used by the culling pipeline)
	VkPipeline _depthPyramidPipeline = VK_NULL_HANDLE;
	VkPipelineLayout _depthPyramidPipelineLayout = VK_NULL_HANDLE;
	ShaderPass _depthPyramidShaderPass;

	// Depth pyramid computing data
	AllocatedImage _depthPyramid = {};
	uint32_t _depthPyramidWidth = 0;
	uint32_t _depthPyramidHeight = 0;
	std::vector<VkImageView> _depthPyramidLevelViews;
	DescriptorAllocator _depthPyramidDescriptorAllocator;  // A separate descriptor allocator. Not really required dont worry about it :)
	std::vector<VkDescriptorSet> _depthPyramidDescriptorSets;
	VkDescriptorSetLayout _depthPyramidDescriptorSetLayout = VK_NULL_HANDLE;
	std::vector<VkImageMemoryBarrier> _depthPyramidMipLevelBarriers;

	// Barriers to wait for the depth buffer to be ready, and one for when the depth pyramid is ready.
	VkImageMemoryBarrier _framebufferDepthWriteBarrier = {};
	VkImageMemoryBarrier _framebufferDepthReadBarrier = {};
	
};

