#pragma once

#include <memory>
#include <unordered_map>

#include <scene/Camera.h>

#include "InputManager.h"
#include "VulkanInstance.h"
#include "VulkanRenderer.h"

namespace leoscene {
	class Scene;
}

class Window;

struct ApplicationState
{
	bool frustumCulling = true;
	bool occlusionCulling = true;
	bool makeAllObjectsTransparent = false;
	bool lockFrustumCullingCamera = false;
};

class Application
{
public:
	Application();
	~Application();

public:
	int init();
	int loadScene(const std::string& filePath);
	int start();
	void cleanup();

private:
	int _initMembers();

private:
	std::unique_ptr<VulkanRenderer> _renderer;
	std::unique_ptr<InputManager> _inputManager;
	std::unique_ptr<VulkanInstance> _vulkan;
	std::unique_ptr<leoscene::Camera> _camera;
	std::unique_ptr<Window> _window;
	std::unique_ptr<ApplicationState> _state;

	friend class InputManager;
};

