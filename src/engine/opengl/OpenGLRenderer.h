#pragma once

#include "../Renderer.h"
#include "Shader.h"

#include <memory>
#include <vector>
#include <map>
#include <unordered_map>

#include <glad/glad.h>

namespace leoscene {
	class Shape;
	class Transform;
}

class Window;

struct OpenGLMaterialUniform
{
};

struct OpenGLShapeData
{
	GLuint VAO = 0;
	GLuint VBO = 0;
	GLuint EBO = 0;
	size_t nbElements = 0;
};

struct OpenGLMaterialData
{
	std::unordered_map<std::string, GLuint> textures;
};

struct OpenGLObjectData
{
	OpenGLObjectData(const glm::mat4& model) : model(model)
	{
	}
	glm::mat4 model;
};

// Aliases to ids for clarity
using materialIdx_t = size_t;
using shapeIdx_t = size_t;

class OpenGLRenderer : public Renderer
{
public:
	OpenGLRenderer(const ApplicationState* applicationState, const leoscene::Camera* camera);

public:
	// Inherited via Renderer
	virtual void init(Window* window) override;
	virtual void cleanup() override;
	virtual void drawFrame() override;
	virtual void loadSceneToRenderer(const leoscene::Scene* scene) override;

	void notifyWindowResize();

private:
	void _resizeViewport(size_t width, size_t height);
	void _updateCamera();
	void _setActiveMaterial(materialIdx_t id);

private:
	Window* _window = nullptr;

	bool _sceneLoaded = false;
	bool _initialized = false;
	bool _viewportNeedsResize = false;

	// Camera-related data
	glm::mat4 _projectionMatrix = glm::mat4(1);
	float _zNear = 0.1f;
	float _zFar = 300.f;

	// OpenGL
	unsigned int shaderProgram;

	std::unique_ptr<Shader> _mainShader;

	std::vector<OpenGLShapeData> _shapeData;
	std::vector<OpenGLMaterialData> _materialData;

	struct _ObjectInstanceData {
		_ObjectInstanceData(const leoscene::Shape* shape, const leoscene::Transform* transform) :
			shape(shape), transform(transform)
		{}

		const leoscene::Shape* shape = nullptr;
		const leoscene::Transform* transform = nullptr;
	};
	std::map<materialIdx_t, std::map<shapeIdx_t, std::vector<_ObjectInstanceData>>> _objectInstances;
	GLuint _objectDataSSBO = 0;
	GLuint _indirectDrawCommandsSSBO = 0;
};