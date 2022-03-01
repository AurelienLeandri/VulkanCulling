#pragma once

#include <ctime>

namespace leoscene {
	class Camera;
}

struct ApplicationState;
class Window;

class Application;
struct GLFWwindow;

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
	void init(Window* window, Application* application, ApplicationState* applicationState);
	void setCamera(leoscene::Camera* camera);
	bool processInput();
	void processMouseMovement(float xoffset, float yoffset);

private:
	void _updateApplicationCamera(CameraMovement direction, float deltaTime);
	void _updateApplicationState(ApplicationToggle toggle);

private:
	static void _mouseCallback(GLFWwindow* window, double xpos, double ypos);
	static void _framebufferSizeCallback(GLFWwindow* window, int width, int height);

private:
	leoscene::Camera* _camera = nullptr;
	Window* _window = nullptr;
	Application* _application;  // TODO: Instead, use an observer for anything that needs to be read from renderers and application. Good enough for now.
	ApplicationState* _applicationState = nullptr;
	std::clock_t _frameClock = std::clock();
	bool _oPressed = false;
	bool _fPressed = false;
	bool _tPressed = false;
	bool _lPressed = false;

private:
	static const float _MOVEMENT_SPEED;
};

