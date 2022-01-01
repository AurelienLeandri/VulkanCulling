#include "Application.h"

#define _USE_MATH_DEFINES
#include <math.h>

#include <iostream>

#include <scene/Scene.h>
#include <scene/SceneLoader.h>

#include "VulkanRenderer.h"
#include "DebugUtils.h"
#include "Window.h"

Application::Application()
{
    _window = std::make_unique<Window>(1600, 1200);
    _inputManager = std::make_unique<InputManager>(*this);
}

Application::~Application() = default;

int Application::init()
{
    if (_initMembers()) {
        std::cerr << "Error: Failed to initialize application." << std::endl;
        return -1;
    }

    return 0;
}

int Application::_initMembers() {
    if (_window->init()) {
        std::cerr << "Error: Failed to create window." << std::endl;
        return -1;
    }

    _inputManager->init(_window->window);
    _camera = std::make_unique<leoscene::Camera>();
    _inputManager->setCamera(_camera.get());

    _vulkan = std::make_unique<VulkanInstance>(_window->window);
    try {
        _vulkan->init();
    } catch (const VulkanRendererException& e) {
        std::cerr << e.what() << std::endl;
        std::cerr << "Error: Failed to initialize Vulkan instance." << std::endl;
        return -1;
    }

    _renderer = std::make_unique<VulkanRenderer>(_vulkan.get());
    try {
        _renderer->init();
    } catch (const VulkanRendererException& e) {
        std::cerr << e.what() << std::endl;
        std::cerr << "Error: Failed to initialize Vulkan renderer." << std::endl;
        return -1;
    }

    return 0;
}

void Application::cleanup()
{
    _renderer->cleanup();
    _vulkan->cleanup();
    _window.reset();
    _camera.reset();
}

int Application::loadScene(const std::string& filePath)
{
    leoscene::Scene scene;

    try {
        leoscene::SceneLoader::loadScene(filePath.c_str(), &scene, _camera.get());
    }
    catch (leoscene::SceneLoaderException e) {
        std::cerr << e.what() << std::endl;
        return -1;
    }

    _renderer->setCamera(_camera.get());
    _renderer->loadSceneToDevice(&scene);

    return 0;
}

int Application::start()
{
    //_renderer->start();
    try {
        while (_inputManager->processInput()) {
            _renderer->iterate();
        }
    } catch (const VulkanRendererException& e) {
        std::cerr << "Vulkan renderer error: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}

int Application::stop()
{
    //_renderer->stop();
    return 0;
}