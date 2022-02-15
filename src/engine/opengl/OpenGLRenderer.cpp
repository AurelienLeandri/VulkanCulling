#include "OpenGLRenderer.h"

OpenGLRenderer::OpenGLRenderer(const ApplicationState* applicationState, const leoscene::Camera* camera) :
	Renderer(applicationState, camera)
{
}

void OpenGLRenderer::init(GLFWwindow* window)
{
}

void OpenGLRenderer::cleanup()
{
}

void OpenGLRenderer::drawFrame()
{
}

void OpenGLRenderer::loadSceneToRenderer(const leoscene::Scene* scene)
{
}
