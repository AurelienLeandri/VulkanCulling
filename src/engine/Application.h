#pragma once

#include <memory>
#include <unordered_map>

#include <scene/Camera.h>

#include "InputManager.h"
#include "vulkan/VulkanInstance.h"
#include "vulkan/VulkanRenderer.h"

namespace leoscene {
	class Scene;
}

class Window;

/*
* Very (very!) simple state machine set by the input manager and read by the renderer.
*/
struct ApplicationState
{
	bool frustumCulling = true;
	bool occlusionCulling = true;
	bool makeAllObjectsTransparent = false;
	bool lockCullingCamera = false;
};

/*
* Entry point of the program. Sets up the scene and the renderers, then launches the renderer.
*/
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
	std::unique_ptr<VulkanRenderer> _renderer;
	std::unique_ptr<InputManager> _inputManager;
	std::unique_ptr<VulkanInstance> _vulkan;
	std::unique_ptr<leoscene::Camera> _camera;
	std::unique_ptr<Window> _window;
	std::unique_ptr<ApplicationState> _state;
};

