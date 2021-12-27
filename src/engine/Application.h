#pragma once

#include <memory>
#include <unordered_map>

#include <scene/Camera.h>

#include "InputManager.h"
#include "VulkanInstance.h"
#include "VulkanRenderer.h"

namespace leo {
	class Scene;
}

class Window;

class Application
{
public:
	Application();
	~Application();

public:
	int init();
	int loadScene(const std::string& filePath);
	int start();
	int stop();

private:
	int _initMembers();
	void _cleanUp();

private:
	std::unique_ptr<VulkanRenderer> _renderer;
	std::unique_ptr<InputManager> _inputManager;
	std::unique_ptr<VulkanInstance> _vulkan;
	std::unique_ptr<leo::Camera> _camera;
	std::unique_ptr<Window> _window;

	friend class InputManager;
};

