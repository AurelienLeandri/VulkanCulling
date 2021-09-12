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

int Application::start()
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
    std::shared_ptr<leo::Scene> scene;
    std::shared_ptr<leo::Camera> camera;

    try {
        leo::SceneLoader::loadScene("../Resources/Models/Sponza/Sponza.scene", scene, camera);
    }
    catch (leo::SceneLoaderException e) {
        return -1;
    }

    if (!scene || !camera) {
        std::cerr << "Error: Failed to load scene and/or camera." << std::endl;
        return -1;
    }

    // TODO: load lights from scene file
    scene->lights.push_back(std::make_shared<leo::DirectionalLight>(glm::vec3(0, -1, 0), glm::vec3(1000)));

    return 0;
}

/*
void Application::mainLoop()
{
    int previouslyActive = _activeRenderer;
    _renderers[_rendererNames[_activeRenderer]]->start();

    while (_inputManager.processInput()) {

        if (_activeRenderer != previouslyActive) {
            _renderers[_rendererNames[previouslyActive]]->stop();
            previouslyActive = _activeRenderer;
            _renderers[_rendererNames[_activeRenderer]]->start();
        }

        _renderers[_rendererNames[_activeRenderer]]->iterate();
    }

    _renderers[_rendererNames[_activeRenderer]]->stop();
}
*/
