#pragma once

#include <memory>
#include <unordered_map>

#include <Scene/Camera.h>

#include "InputManager.h"
#include "VulkanInstance.h"
#include "VulkanRenderer.h"

namespace leo {
	class Scene;
}

class Application
{
public:
	Application();
	~Application();

public:
	int start();
	int loadScene(const std::string& filePath);
	int loadScene(leo::Scene* scene);
	void setCamera(const leo::Camera& camera);
	void mainLoop();

private:
	int _initWindow();
	int _initRenderers(std::string& failed);
	void _cleanUp();

private:
	static const unsigned int DEFAULT_WINDOW_WIDTH = 1200;
	static const unsigned int DEFAULT_WINDOW_HEIGHT = 1200;

private:
	std::unordered_map<std::string, std::unique_ptr<VulkanRenderer>> _renderers;
	InputManager _inputManager;
	VulkanInstance _vulkan;
	std::shared_ptr<leo::Camera> _camera;
	GLFWwindow* _window = nullptr;
	leo::Scene* _scene = nullptr;
	std::vector<std::string> _rendererNames;
	unsigned int _activeRenderer = -1;

	friend class InputManager;
};

