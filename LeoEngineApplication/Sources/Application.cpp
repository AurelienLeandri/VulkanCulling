#include "Application.h"

#define _USE_MATH_DEFINES
#include <math.h>

#include <iostream>

#include <Scene/Scene.h>
#include <Scene/ResourcesLoading/SceneLoader.h>
#include <Scene/Lights/DirectionalLight.h>

#include "VulkanRenderer.h"
#include "Window.h"

Application::Application()
{
}

Application::~Application()
{
    _cleanUp();
}

int Application::init()
{
    if (_initMembers()) {
        std::cerr << "Error: Failed to initialize application." << std::endl;
        return -1;
    }

    return 0;
}

int Application::_initMembers() {
    _window = std::make_unique<Window>(800, 600);
    if (_window->init()) {
        std::cerr << "Error: Failed to create window." << std::endl;
        return -1;
    }

    _inputManager = std::make_unique<InputManager>(*this);
    _inputManager->init();

    _vulkan = std::make_unique<VulkanInstance>(_window->window);
    if (_vulkan->init()) {
        std::cerr << "Error: Failed to initialize Vulkan instance." << std::endl;
        return -1;
    }
}

void Application::_cleanUp()
{
    _renderer.reset();
    _vulkan.reset();
    _window.reset();
    _camera.reset();
    _scene.reset();
}

int Application::loadScene(const std::string& filePath)
{
    _scene = std::make_unique<leo::Scene>();
    _camera = std::make_unique<leo::Camera>(glm::vec3(0), glm::vec3(0, 0, 1), glm::vec3(0, 1, 0), glm::radians(90.f));

    try {
        leo::SceneLoader::loadScene(filePath.c_str(), _scene.get(), _camera.get());
    }
    catch (leo::SceneLoaderException e) {
        return -1;
    }

    // TODO: load lights from scene file
    _scene->lights.push_back(std::make_shared<leo::DirectionalLight>(glm::vec3(0, -1, 0), glm::vec3(1000)));

    return 0;
}

int Application::start()
{
    //_renderer->start();

    while (_inputManager->processInput()) {
        //_renderer->iterate();
    }

    return 0;
}

int Application::stop()
{
    //_renderer->stop();
    return 0;
}
