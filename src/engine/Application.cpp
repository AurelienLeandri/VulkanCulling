#include "Application.h"

#define _USE_MATH_DEFINES
#include <math.h>

#include <iostream>

#include <scene/Scene.h>
#include <scene/SceneLoader.h>

#include "vulkan/VulkanRenderer.h"
#include "Window.h"

Application::Application()
{
    _window = std::make_unique<Window>(1600, 1200);
    _inputManager = std::make_unique<InputManager>();
    _state = std::make_unique<ApplicationState>();
    _camera = std::make_unique<leoscene::Camera>(glm::vec3(0, -3, 0), glm::vec3(1, 0, 0), glm::vec3(0, -1, 0), glm::radians(90.f));
}

Application::~Application() = default;

int Application::init()
{
    if (_window->init()) {
        std::cerr << "Error: Failed to create window." << std::endl;
        return -1;
    }

    _inputManager->init(_window->window, _state.get());
    _inputManager->setCamera(_camera.get());

    _renderers["VulkanRenderer"] = std::make_unique<VulkanRenderer>(_state.get(), _camera.get());
    _activeRenderer = _renderers["VulkanRenderer"].get();

    for (const std::pair<const std::string, std::unique_ptr<Renderer>>& renderersPair : _renderers) {
        try {
            renderersPair.second->init(_window->window);
        }
        catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
            std::cerr << "Error: Failed to initialize " << renderersPair.first << "." << std::endl;
            return -1;
        }
    }
    

    return 0;
}

void Application::cleanup()
{
    for (const std::pair<const std::string, std::unique_ptr<Renderer>>& renderersPair : _renderers) {
        try {
            renderersPair.second->cleanup();
        }
        catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
            std::cerr << "Error: Failed to cleanup " << renderersPair.first << "." << std::endl;
        }
    }
}

int Application::loadScene(const std::string& filePath)
{
    leoscene::Scene scene;
    leoscene::SceneLoader sceneLoader;

    try {
        sceneLoader.loadScene(filePath.c_str(), &scene, _camera.get());
    }
    catch (leoscene::SceneLoaderException e) {
        std::cerr << e.what() << std::endl;
        return -1;
    }

    try {
        _activeRenderer->loadSceneToRenderer(&scene); // Whenever we allow having two real time renderers activated at the same time, scene should be loaded in both. TBD.
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return -1;
    }

    return 0;
}

int Application::start()
{
    try {
        while (_inputManager->processInput()) {
            _activeRenderer->drawFrame();
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return -1;
    }

    return 0;
}
