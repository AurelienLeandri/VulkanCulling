#include "OpenGLRenderer.h"

#include "../Window.h"

#include "OpenGLError.h"

#include <scene/Scene.h>
#include <scene/PerformanceMaterial.h>
#include <scene/Mesh.h>
#include <scene/Transform.h>
#include <scene/Camera.h>
#include <scene/GeometryIncludes.h>

#include <glad/glad.h>
#include <glfw/glfw3.h>

#include <string>
#include <map>
#include <array>

#include <glm/glm.hpp>

namespace {
    static void APIENTRY _glDebugOutput(GLenum source,
        GLenum type,
        unsigned int id,
        GLenum severity,
        GLsizei length,
        const char* message,
        const void* userParam);
}

OpenGLRenderer::OpenGLRenderer(const ApplicationState* applicationState, const leoscene::Camera* camera) :
	Renderer(applicationState, camera)
{
}

void OpenGLRenderer::init(Window* window)
{
	_window = window;

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		throw OpenGLRendererException("GLAD: Failed to load GL loader.");
	}

#ifdef _DEBUG
    {
        int flags = 0;
        // Check if the debug context is active
        GL_CHECK(glGetIntegerv(GL_CONTEXT_FLAGS, &flags));
        if (flags & GL_CONTEXT_FLAG_DEBUG_BIT)
        {
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDebugMessageCallback(_glDebugOutput, nullptr);
            glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
        }
        else {
            throw OpenGLRendererException("Failed to initialize DebugOutput for OpenGL.");
        }
    }
#endif

    GL_CHECK(glViewport(0, 0, static_cast<GLsizei>(_window->width), static_cast<GLsizei>(_window->height)));

	/*
	* Global options
	*/

    GL_CHECK(glEnable(GL_DEPTH_TEST));

	/*
	* Shaders
	*/

	_mainShader = std::make_unique<Shader>("resources/shaders/opengl/main.vert", "resources/shaders/opengl/main.frag");

	/*
	* Some global data
	*/

	_projectionMatrix = glm::perspective(glm::radians(45.0f), static_cast<float>(_window->width) / _window->width, _zNear, _zFar);

	/*
	* Input data buffers
	*/

    /*
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);

	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	glGenBuffers(1, &EBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	glBindVertexArray(0);
    */

	_initialized = true;
}

void OpenGLRenderer::cleanup()
{
	if (!_initialized) return;
}

void OpenGLRenderer::drawFrame()
{
	if (_viewportNeedsResize) {
		_resizeViewport(_window->width, _window->height);
	}

    GL_CHECK(glClearColor(0.2f, 0.3f, 0.3f, 1.0f));
    GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

	_mainShader->use();
	_updateCamera();

    size_t baseInstanceIndex = 0;
    for (const std::pair<materialIdx_t, std::map<shapeIdx_t, std::vector<_ObjectInstanceData>>>& entriesPerMaterial : _objectInstances) {
        _setActiveMaterial(entriesPerMaterial.first);
        for (const std::pair<shapeIdx_t, std::vector<_ObjectInstanceData>>& entriesPerShapePerMaterial : entriesPerMaterial.second) {
            GLsizei nbInstances = static_cast<GLsizei>(entriesPerShapePerMaterial.second.size());
            shapeIdx_t shapeIdx = entriesPerShapePerMaterial.first;
            GL_CHECK(glBindVertexArray(_shapeData[shapeIdx].VAO));
            GL_CHECK(glDrawElementsInstancedBaseInstance(GL_TRIANGLES, static_cast<GLsizei>(_shapeData[shapeIdx].nbElements), GL_UNSIGNED_INT,
                nullptr, nbInstances, baseInstanceIndex));
            baseInstanceIndex += nbInstances;
        }
    }
    GL_CHECK(glBindVertexArray(0));

	glfwSwapBuffers(_window->window);
}

void OpenGLRenderer::_setActiveMaterial(materialIdx_t id)
{
    const OpenGLMaterialData& material = _materialData[id];
    GLuint i = 0;
    for (const auto& [uniformName, textureId] : material.textures) {
        glActiveTexture(GL_TEXTURE0 + i);
        _mainShader->setInt(uniformName, i);
        glBindTexture(GL_TEXTURE_2D, textureId);
        ++i;
    }
    GL_CHECK(glActiveTexture(GL_TEXTURE0));
}


void OpenGLRenderer::_updateCamera()
{
	glm::vec3 front = _camera->getFront();
	glm::vec3 up = _camera->getUp();
	glm::vec3 position = _camera->getPosition();
    position.y *= -1;

	glm::mat4 view = glm::lookAtRH(position, position + front, -up);
	
	_mainShader->setMat("view", view);
	_mainShader->setMat("proj", _projectionMatrix);
	_mainShader->setMat("viewProj", _projectionMatrix * view);
}


void OpenGLRenderer::loadSceneToRenderer(const leoscene::Scene* scene)
{
    /*
    * Loading scene objects to device
    */

    {
        std::map<const leoscene::Material*, materialIdx_t> loadedMaterialsCache;
        std::map<const leoscene::ImageTexture*, GLuint> loadedImagesCache;
        std::map<const leoscene::Shape*, shapeIdx_t> shapeDataCache;

        for (const leoscene::SceneObject& sceneObject : scene->objects) {
            const leoscene::PerformanceMaterial* sceneMaterial = static_cast<const leoscene::PerformanceMaterial*>(sceneObject.material.get());
            const leoscene::Shape* sceneShape = sceneObject.shape.get();
            materialIdx_t loadedMaterialIdx = 0;
            shapeIdx_t loadedShapeIdx = 0;

            // Load material data on the device
            if (loadedMaterialsCache.find(sceneMaterial) == loadedMaterialsCache.end()) {
                _materialData.emplace_back();
                loadedMaterialIdx = _materialData.size() - 1;

                static const size_t nbTexturesInMaterial = 5;
                std::array<std::pair<std::string, const leoscene::ImageTexture*>, nbTexturesInMaterial> materialTextures{
                    std::make_pair<std::string, const leoscene::ImageTexture*>("diffuseTexture", sceneMaterial->diffuseTexture.get()),
                    std::make_pair<std::string, const leoscene::ImageTexture*>("specularTexture", sceneMaterial->specularTexture.get()),
                    std::make_pair<std::string, const leoscene::ImageTexture*>("ambientTexture", sceneMaterial->ambientTexture.get()),
                    std::make_pair<std::string, const leoscene::ImageTexture*>("normalTexture", sceneMaterial->normalsTexture.get()),
                    std::make_pair<std::string, const leoscene::ImageTexture*>("heightTexture", sceneMaterial->heightTexture.get()),
                };

                for (size_t i = 0; i < nbTexturesInMaterial; ++i) {
                    const leoscene::ImageTexture* sceneTexture = materialTextures[i].second;
                    GLuint loadedImage = 0;
                    if (loadedImagesCache.find(sceneTexture) == loadedImagesCache.end()) {

                        uint32_t texWidth = static_cast<uint32_t>(sceneTexture->width);
                        uint32_t texHeight = static_cast<uint32_t>(sceneTexture->height);

                        uint32_t nbChannels = 0;
                        GLenum imageFormat = 0;
                        switch (sceneTexture->layout) {
                        case leoscene::ImageTexture::Layout::R:
                            imageFormat = GL_RED;
                            nbChannels = 1;
                            break;
                        case leoscene::ImageTexture::Layout::RGBA:
                            if (i == 3) { // Normals texture
                                imageFormat = GL_RGBA;  // TODO: Might need a change later
                            }
                            else {
                                imageFormat = GL_RGBA;
                            }
                            nbChannels = 4;
                            break;
                        default:
                            break;
                        }

                        if (!nbChannels || !imageFormat) {
                            throw OpenGLRendererException("A texture on a sceneMaterial has a format that is not expected. Something is very very wrong.");
                        }

                        // Image handle and memory

                        glGenTextures(1, &loadedImage);
                        glBindTexture(GL_TEXTURE_2D, loadedImage);
                        glTexImage2D(GL_TEXTURE_2D, 0, imageFormat, texWidth, texHeight, 0, imageFormat, GL_UNSIGNED_BYTE, sceneTexture->data);
                        glGenerateMipmap(GL_TEXTURE_2D);

                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

                        _materialData[loadedMaterialIdx].textures[materialTextures[i].first] = loadedImage;

                        loadedImagesCache[sceneTexture] = loadedImage;
                    }
                    else {
                        loadedImage = loadedImagesCache[sceneTexture];
                    }
                }
                loadedMaterialsCache[sceneMaterial] = loadedMaterialIdx;
            }
            else {
                loadedMaterialIdx = loadedMaterialsCache[sceneMaterial];
            }


            /*
            * Load shape data on the device
            */

            if (shapeDataCache.find(sceneShape) == shapeDataCache.end()) {
                _shapeData.emplace_back();
                loadedShapeIdx = _shapeData.size() - 1;
                OpenGLShapeData& loadedShape = _shapeData[loadedShapeIdx];

                const leoscene::Mesh* mesh = static_cast<const leoscene::Mesh*>(sceneShape);  // TODO: assuming the shape is a mesh for now

                GL_CHECK(glGenVertexArrays(1, &loadedShape.VAO));
                GL_CHECK(glGenBuffers(1, &loadedShape.VBO));
                GL_CHECK(glGenBuffers(1, &loadedShape.EBO));

                GL_CHECK(glBindVertexArray(loadedShape.VAO));
                {
                    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, loadedShape.VBO));
                    GL_CHECK(glBufferData(GL_ARRAY_BUFFER, mesh->vertices.size() * sizeof(leoscene::Vertex), mesh->vertices.data(), GL_STATIC_DRAW));

                    GL_CHECK(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(leoscene::Vertex), static_cast<void*>(0)));
                    GL_CHECK(glEnableVertexAttribArray(0));
                    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(leoscene::Vertex), (void*)(3 * sizeof(float)));
                    glEnableVertexAttribArray(1);
                    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(leoscene::Vertex), (void*)(6 * sizeof(float)));
                    glEnableVertexAttribArray(2);
                    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(leoscene::Vertex), (void*)(9 * sizeof(float)));
                    glEnableVertexAttribArray(3);

                    GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, loadedShape.EBO));
                    GL_CHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->indices.size() * sizeof(uint32_t), mesh->indices.data(), GL_STATIC_DRAW));
                }
                GL_CHECK(glBindVertexArray(0));

                loadedShape.nbElements = mesh->indices.size();

                shapeDataCache[sceneShape] = loadedShapeIdx;
            }
            else {
                loadedShapeIdx = shapeDataCache[sceneShape];
            }


            /*
            * Incrementing the counter for the given pair of material and shape data.
            */
            /*

            if (objectInstances.find(loadedMaterial) == objectInstances.end()) {
                objectInstances[loadedMaterial] = {};
            }
            if (objectInstances[loadedMaterial].find(loadedShapeIdx) == objectInstances[loadedMaterial].end()) {
                objectInstances[loadedMaterial][loadedShapeIdx] = {};
            }
            objectInstances[loadedMaterial][loadedShapeIdx].push_back({ sceneShape, sceneObject.transform.get() });
            */

            // TODO: instead of zero, put the material id
            if (_objectInstances.find(loadedMaterialIdx) == _objectInstances.end()) {
                _objectInstances[loadedMaterialIdx] = {};
            }
            _objectInstances[loadedMaterialIdx][loadedShapeIdx].emplace_back(sceneShape, sceneObject.transform.get());
        }

    }

    //_nbMaterials = objectInstances.size();


    /*
    * Initializing the object batches used to compute draw indirect commands
    */

    /*
    for (const auto& materialShapesPair : objectInstances) {
        const Material* material = materialShapesPair.first;  // TODO: assuming material is Performance for now
        for (const auto& shapeNbPair : materialShapesPair.second) {
            const ShapeData* shape = shapeNbPair.first;  // TODO: assuming the shape is a mesh for now

            _drawCalls.push_back({ material, shape,
                static_cast<uint32_t>(shapeNbPair.second.size()),  // nbObjects
                shape->nbElements, // primitivesPerObject
                });
        }
    }
    */
    
    ///*
    //* Filling global scene data
    //*/

    //// TODO: Put actual values (maybe from options and/or leoscene::Scene)
    //GPUSceneData sceneData;
    //sceneData.ambientColor = { 1, 0, 0, 0 };
    //sceneData.sunlightColor = { 0, 1, 0, 0 };
    //sceneData.sunlightDirection = { 0, 0, 0, 1 };
    //_vulkan.copyDataToBuffer(sizeof(GPUSceneData), _sceneDataBuffer, &sceneData);

    ///*
    //* Per-object data
    //*/

    //{
    //    size_t objectsDataBufferSize = scene->objects.size() * sizeof(GPUObjectData);
    //    if (!objectsDataBufferSize) {
    //        throw VulkanRendererException("The scene does not contain any objects!");
    //    }

    //    AllocatedBuffer stagingBuffer;
    //    _vulkan.createBuffer(objectsDataBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, stagingBuffer);

    //    GPUObjectData* objectDataPtr = static_cast<GPUObjectData*>(_vulkan.mapBuffer(stagingBuffer));
    //    int i = 0;
    //    for (const auto& materialShapesPair : objectInstances) {
    //        const Material* material = materialShapesPair.first;  // TODO: assuming material is Performance for now
    //        for (const auto& shapeNbPair : materialShapesPair.second) {
    //            const ShapeData* shape = shapeNbPair.first;  // TODO: assuming the shape is a mesh for now
    //            for (const _ObjectInstanceData& instanceData : shapeNbPair.second) {
    //                const glm::mat4& modelMatrix = instanceData.transform->getMatrix();
    //                objectDataPtr[i].modelMatrix = modelMatrix;

    //                // Computing sphere bounds of the object in world space
    //                const glm::vec4& sphereBounds = static_cast<const leoscene::Mesh*>(instanceData.shape)->boundingSphere;
    //                glm::vec4 transformedSphere = modelMatrix * glm::vec4(sphereBounds.x, sphereBounds.y, sphereBounds.z, 1);
    //                float maxScale = glm::max(glm::max(glm::length(modelMatrix[0]), glm::length(modelMatrix[1])), glm::length(modelMatrix[2]));
    //                transformedSphere.w = maxScale * sphereBounds.w;
    //                objectDataPtr[i].sphereBounds = transformedSphere;

    //                i++;
    //            }
    //        }
    //    }
    //    _vulkan.unmapBuffer(stagingBuffer);

    //    _vulkan.createBuffer(objectsDataBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, _objectsDataBuffer);
    //    _vulkan.copyBufferToBuffer(_mainCommandPool, stagingBuffer.buffer, _objectsDataBuffer.buffer, objectsDataBufferSize);
    //    _vulkan.destroyBuffer(stagingBuffer);
    //}

    /*
    * Per-object data
    */

    {
        std::vector<OpenGLObjectData> gpuObjectData;
        for (const std::pair<materialIdx_t, std::map<shapeIdx_t, std::vector<_ObjectInstanceData>>>& entriesPerMaterial : _objectInstances) {
            materialIdx_t materialId = entriesPerMaterial.first;
            for (const std::pair<shapeIdx_t, std::vector<_ObjectInstanceData>>& entriesPerShapePerMaterial : entriesPerMaterial.second) {
                shapeIdx_t shapeIdx = entriesPerShapePerMaterial.first;
                for (const _ObjectInstanceData& objectInstanceData : entriesPerShapePerMaterial.second) {
                    gpuObjectData.emplace_back(objectInstanceData.transform ? objectInstanceData.transform->getMatrix() : glm::mat4(1));
                }
            }
        }

        GL_CHECK(glGenBuffers(1, &_objectDataSSBO));
        GL_CHECK(glBindBuffer(GL_SHADER_STORAGE_BUFFER, _objectDataSSBO));
        GL_CHECK(glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(OpenGLObjectData) * gpuObjectData.size(), gpuObjectData.data(), GL_STATIC_COPY));
        GL_CHECK(glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _objectDataSSBO));
    }

    ///*
    //* Indirect Command buffer
    //*/

    //std::vector<GPUIndirectDrawCommand> commandBufferData(_drawCalls.size(), GPUIndirectDrawCommand{});
    //uint32_t offset = 0;
    //for (int i = 0; i < _drawCalls.size(); ++i) {
    //    GPUIndirectDrawCommand& gpuBatch = commandBufferData[i];
    //    gpuBatch.command.firstInstance = offset;  // Used to access i in the model matrix since we dont use instancing.
    //    gpuBatch.command.instanceCount = 0;
    //    gpuBatch.command.indexCount = _drawCalls[i].primitivesPerObject;
    //    _nbInstances += _drawCalls[i].nbObjects;
    //    offset += _drawCalls[i].nbObjects;
    //}

    //_vulkan.createGPUBufferFromCPUData(_mainCommandPool, commandBufferData.size() * sizeof(GPUIndirectDrawCommand),
    //    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
    //    commandBufferData.data(),
    //    _gpuBatches
    //);


    ///*
    //* Indirect Command buffer reset
    //*/

    //_vulkan.createGPUBufferFromCPUData(_mainCommandPool, commandBufferData.size() * sizeof(GPUIndirectDrawCommand),
    //    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    //    commandBufferData.data(),
    //    _gpuResetBatches
    //);


    ///*
    //* Instances buffer
    //*/

    //_totalInstancesNb = static_cast<uint32_t>(scene->objects.size());

    //std::vector<GPUObjectInstance> objects(_totalInstancesNb);
    //{
    //    uint32_t entryIdx = 0;
    //    for (uint32_t batchIdx = 0; batchIdx < _drawCalls.size(); ++batchIdx) {
    //        for (uint32_t i = 0; i < _drawCalls[batchIdx].nbObjects; ++i) {
    //            objects[entryIdx].batchId = batchIdx;
    //            objects[entryIdx].dataId = entryIdx;
    //            entryIdx++;
    //        }
    //    }
    //}
    //_vulkan.createGPUBufferFromCPUData(_mainCommandPool, _totalInstancesNb * sizeof(GPUObjectInstance),
    //    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    //    objects.data(),
    //    _gpuObjectInstances
    //);

    //_vulkan.createGPUBufferFromCPUData(_mainCommandPool, _totalInstancesNb * sizeof(uint32_t),
    //    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    //    objects.data(),
    //    _gpuIndexToObjectId
    //);


    ///*
    //* Culling global data buffer
    //*/

    //GPUCullingGlobalData globalData;
    //glm::mat4 projectionT = glm::transpose(_projectionMatrix);
    //globalData.frustum[0] = projectionT[3] + projectionT[0];
    //globalData.frustum[1] = projectionT[3] - projectionT[0];
    //globalData.frustum[2] = projectionT[3] + projectionT[1];
    //globalData.frustum[3] = projectionT[3] - projectionT[1];
    //globalData.zNear = _zNear;
    //globalData.zFar = _zFar;
    //globalData.P00 = projectionT[0][0];
    //globalData.P11 = projectionT[1][1];
    //globalData.pyramidWidth = _depthPyramidWidth;
    //globalData.pyramidHeight = _depthPyramidHeight;
    //globalData.nbInstances = _totalInstancesNb;

    //_vulkan.createGPUBufferFromCPUData(_mainCommandPool, sizeof(GPUCullingGlobalData),
    //    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    //    &globalData,
    //    _gpuCullingGlobalData
    //);


    ///*
    //* Setup descriptors. Global descriptors depend partially on the scene being loaded, so we create them here.
    //*/

    //_createGlobalDescriptors(_totalInstancesNb);
    //_createCullingDescriptors(_totalInstancesNb);


    ///*
    //* Culling data barriers
    //*/

    //_gpuIndexToObjectIdBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    //_gpuIndexToObjectIdBarrier.pNext = nullptr;
    //_gpuIndexToObjectIdBarrier.buffer = _gpuIndexToObjectId.buffer;
    //_gpuIndexToObjectIdBarrier.size = VK_WHOLE_SIZE;
    //_gpuIndexToObjectIdBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    //_gpuIndexToObjectIdBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    //_gpuIndexToObjectIdBarrier.srcQueueFamilyIndex = static_cast<uint32_t>(_vulkan.getQueueFamilyIndices().graphicsFamily.value());

    //_gpuBatchesBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    //_gpuBatchesBarrier.pNext = nullptr;
    //_gpuBatchesBarrier.buffer = _gpuBatches.buffer;
    //_gpuBatchesBarrier.size = VK_WHOLE_SIZE;
    //_gpuBatchesBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    //_gpuBatchesBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    //_gpuBatchesBarrier.srcQueueFamilyIndex = static_cast<uint32_t>(_vulkan.getQueueFamilyIndices().graphicsFamily.value());

    //_gpuBatchesResetBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    //_gpuBatchesResetBarrier.pNext = nullptr;
    //_gpuBatchesResetBarrier.buffer = _gpuBatches.buffer;
    //_gpuBatchesResetBarrier.size = VK_WHOLE_SIZE;
    //_gpuBatchesResetBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    //_gpuBatchesResetBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
    //_gpuBatchesResetBarrier.srcQueueFamilyIndex = static_cast<uint32_t>(_vulkan.getQueueFamilyIndices().graphicsFamily.value());

    //_updateDynamicData();
    //

    _sceneLoaded = true;
}

void OpenGLRenderer::notifyWindowResize()
{
	_viewportNeedsResize = true;
}

void OpenGLRenderer::_resizeViewport(size_t width, size_t height)
{
    GL_CHECK(glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height)));
	_viewportNeedsResize = false;
}

namespace {
    void APIENTRY _glDebugOutput(
        GLenum source,
        GLenum type,
        unsigned int id,
        GLenum severity,
        GLsizei length,
        const char* message,
        const void* userParam)
    {
        // ignore non-significant error/warning codes
        if (id == 131169 || id == 131185 || id == 131218 || id == 131204) return;

        std::cerr << "---------------" << std::endl;
        std::cerr << "Debug message (" << id << "): " << message << std::endl;

        switch (source)
        {
        case GL_DEBUG_SOURCE_API:             std::cerr << "Source: API"; break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   std::cerr << "Source: Window System"; break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER: std::cerr << "Source: Shader Compiler"; break;
        case GL_DEBUG_SOURCE_THIRD_PARTY:     std::cerr << "Source: Third Party"; break;
        case GL_DEBUG_SOURCE_APPLICATION:     std::cerr << "Source: Application"; break;
        case GL_DEBUG_SOURCE_OTHER:           std::cerr << "Source: Other"; break;
        } std::cerr << std::endl;

        switch (type)
        {
        case GL_DEBUG_TYPE_ERROR:               std::cerr << "Type: Error"; break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: std::cerr << "Type: Deprecated Behaviour"; break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  std::cerr << "Type: Undefined Behaviour"; break;
        case GL_DEBUG_TYPE_PORTABILITY:         std::cerr << "Type: Portability"; break;
        case GL_DEBUG_TYPE_PERFORMANCE:         std::cerr << "Type: Performance"; break;
        case GL_DEBUG_TYPE_MARKER:              std::cerr << "Type: Marker"; break;
        case GL_DEBUG_TYPE_PUSH_GROUP:          std::cerr << "Type: Push Group"; break;
        case GL_DEBUG_TYPE_POP_GROUP:           std::cerr << "Type: Pop Group"; break;
        case GL_DEBUG_TYPE_OTHER:               std::cerr << "Type: Other"; break;
        } std::cerr << std::endl;

        switch (severity)
        {
        case GL_DEBUG_SEVERITY_HIGH:         std::cerr << "Severity: High"; break;
        case GL_DEBUG_SEVERITY_MEDIUM:       std::cerr << "Severity: Medium"; break;
        case GL_DEBUG_SEVERITY_LOW:          std::cerr << "Severity: Low"; break;
        case GL_DEBUG_SEVERITY_NOTIFICATION: std::cerr << "Severity: Notification"; break;
        } std::cerr << std::endl;
        std::cerr << std::endl;

        throw OpenGLRendererException("Error: DebugOutput callback called while using the OpenGL API.");
    }
}

