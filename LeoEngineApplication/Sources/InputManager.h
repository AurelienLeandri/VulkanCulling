#pragma once

#include <glad/glad.h>

#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>

#include <ctime>

namespace leo {
	class Camera;
}

class Application;

class InputManager
{
private:
	enum class CameraMovement {
		FORWARD,
		BACKWARD,
		LEFT,
		RIGHT,
		UP,
		DOWN
	};

public:
	InputManager(Application& application);

public:
	void init(GLFWwindow* window);
	void setCamera(leo::Camera* camera);
	bool processInput();
	void processMouseMovement(float xoffset, float yoffset);

public:
	// TODO: refactor this
	static bool framebufferResized;

private:
	void _processKeyboard(CameraMovement direction, float deltaTime);

private:
	static void _framebufferResizeCallback(GLFWwindow* window, int width, int height);
	static void _mouseCallback(GLFWwindow* window, double xpos, double ypos);

private:
	Application& _application;
	leo::Camera* _camera = nullptr;
	GLFWwindow* _window = nullptr;
	std::clock_t _frameClock = std::clock();
	float _currentYaw = 0;
	float _currentPitch = 0;

private:
	static const float _MOVEMENT_SPEED;
};

