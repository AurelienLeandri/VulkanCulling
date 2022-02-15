#pragma once

#include "../Renderer.h"
#include "Shader.h"

#include <memory>

class Window;

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
	unsigned int VAO;
	unsigned int VBO;
	unsigned int EBO;
	unsigned int shaderProgram;

	std::unique_ptr<Shader> _mainShader;
};