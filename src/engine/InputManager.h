#pragma once

#define GLFW_INCLUDE_VULKAN
#include <glfw/glfw3.h>

#include <ctime>

namespace leoscene {
	class Camera;
}

struct ApplicationState;

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

	enum class ApplicationToggle {
		FRUSTUM_CULLING,
		OCCLUSION_CULLING,
		MAKE_ALL_OBJECTS_TRANSPARENT,
		LOCK_FRUSTUM_CULLING_CAMERA
	};

public:
	InputManager();

public:
	void init(GLFWwindow* window, ApplicationState* applicationState);
	void setCamera(leoscene::Camera* camera);
	bool processInput();
	void processMouseMovement(float xoffset, float yoffset);

private:
	void _updateApplicationCamera(CameraMovement direction, float deltaTime);
	void _updateApplicationState(ApplicationToggle toggle);

private:
	static void _mouseCallback(GLFWwindow* window, double xpos, double ypos);

private:
	leoscene::Camera* _camera = nullptr;
	GLFWwindow* _window = nullptr;
	ApplicationState* _applicationState = nullptr;
	std::clock_t _frameClock = std::clock();
	float _currentYaw = 0;
	float _currentPitch = 0;
	bool _oPressed = false;
	bool _fPressed = false;
	bool _tPressed = false;
	bool _lPressed = false;

private:
	static const float _MOVEMENT_SPEED;
};

