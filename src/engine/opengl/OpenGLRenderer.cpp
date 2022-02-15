#include "OpenGLRenderer.h"

#include "../Window.h"

#include "OpenGLError.h"

#include <glad/glad.h>
#include <glfw/glfw3.h>

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

	glViewport(0, 0, static_cast<GLsizei>(_window->width), static_cast<GLsizei>(_window->height));

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

	glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	glfwSwapBuffers(_window->window);
}

void OpenGLRenderer::loadSceneToRenderer(const leoscene::Scene* scene)
{
	_sceneLoaded = true;
}

void OpenGLRenderer::notifyWindowResize()
{
	_viewportNeedsResize = true;
}

void OpenGLRenderer::_resizeViewport(size_t width, size_t height)
{
	glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
	_viewportNeedsResize = false;
}

