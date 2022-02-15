#include "OpenGLRenderer.h"

#include "../Window.h"

#include "OpenGLError.h"

#include <scene/Camera.h>

#include <glad/glad.h>
#include <glfw/glfw3.h>

#include <string>

#include <glm/glm.hpp>

const char* vertexShaderSource = "#version 330 core\n"
"layout (location = 0) in vec3 aPos;\n"
"void main()\n"
"{\n"
"   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
"}\0";

const char* fragmentShaderSource = "#version 330 core\n"
"out vec4 FragColor;\n"
"void main()\n"
"{\n"
"   FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);\n"
"}\n\0";

float vertices[] = {
	 0.5f,  0.5f, 0.0f,  // top right
	 0.5f, -0.5f, 0.0f,  // bottom right
	-0.5f, -0.5f, 0.0f,  // bottom left
	-0.5f,  0.5f, 0.0f   // top left 
};

unsigned int indices[] = {  // note that we start from 0!
	0, 1, 3,   // first triangle
	1, 2, 3    // second triangle
};

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

	/*
	* Global options
	*/

	glEnable(GL_DEPTH_TEST);

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
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	_mainShader->use();
	_updateCamera();
	_mainShader->setMat("model", glm::mat4(1));
	glBindVertexArray(VAO);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

	glBindVertexArray(0);

	glfwSwapBuffers(_window->window);
}

void OpenGLRenderer::_updateCamera()
{
	glm::vec3 front = _camera->getFront();
	glm::vec3 up = _camera->getUp();
	glm::vec3 position = _camera->getPosition();
	position.y *= -1;

	glm::mat4 view = glm::lookAt(position, position + front, up);
	
	_mainShader->setMat("view", view);
	_mainShader->setMat("proj", _projectionMatrix);
	_mainShader->setMat("viewProj", _projectionMatrix * view);
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

