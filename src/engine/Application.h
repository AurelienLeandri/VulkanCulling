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
	Renderer* activeRenderer = nullptr;
};

/*
* Entry point of the program. Sets up the scene and the renderers, then launches the renderer.
*/
class Application
{
public:
	struct Options {
		std::string startingRenderer = "VulkanRenderer";
	};

public:
	Application();
	~Application();

public:
	int init(Options options = {});
	int loadScene(const std::string& filePath);
	int start();
	void cleanup();

	void notifyWindowResize();

private:
	std::unordered_map<std::string, std::unique_ptr<Renderer>> _renderers;

	std::unique_ptr<InputManager> _inputManager;
	std::unique_ptr<leoscene::Camera> _camera;
	std::unique_ptr<Window> _window;
	std::unique_ptr<ApplicationState> _state;
};

