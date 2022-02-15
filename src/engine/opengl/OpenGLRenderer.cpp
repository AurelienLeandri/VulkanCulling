#include "OpenGLRenderer.h"

#include "OpenGLError.h"

#include <glad/glad.h>
#include <glfw/glfw3.h>

OpenGLRenderer::OpenGLRenderer(const ApplicationState* applicationState, const leoscene::Camera* camera) :
	Renderer(applicationState, camera)
{
}

void OpenGLRenderer::init(GLFWwindow* window)
{
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		throw OpenGLRendererException("GLAD: Failed to load GL loader.");
	}

	glViewport(0, 0, 1600, 1200);  // TODO: get from application window

	_initialized = true;
}

void OpenGLRenderer::cleanup()
{
	if (!_initialized) return;
}

void OpenGLRenderer::drawFrame()
{
}

void OpenGLRenderer::loadSceneToRenderer(const leoscene::Scene* scene)
{
	_sceneLoaded = true;
}
