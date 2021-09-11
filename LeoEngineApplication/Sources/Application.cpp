#include "Application.h"

#define _USE_MATH_DEFINES
#include <math.h>

#include <iostream>

#include "VulkanRenderer.h"
#include <Scene/Scene.h>

Application::Application() :
    _inputManager(*this)
{
}

Application::~Application()
{
    _vulkan.waitForIdleDevice();

    _cleanUp();

    glfwDestroyWindow(_window);
    _window = nullptr;

    glfwTerminate();
}

int Application::start()
{
    if (_initWindow()) {
        std::cerr << "Failed to initialize window context" << std::endl;
        return -1;
    }

    _inputManager.init(_window, &_camera);

    if (_vulkan.init(_window)) {
        std::cerr << "Failed to initialize Vulkan instance" << std::endl;
        return -1;
    }

    std::string failed;
    if (_initRenderers(failed)) {
        std::cerr << "Failed to initialize renderer " << failed << std::endl;
        return -1;
    }

    return 0;
}

int Application::_initWindow()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    _window = glfwCreateWindow(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, "AurellEngine", nullptr, nullptr);

    if (_window == NULL)
    {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(_window);

    return 0;
}

int Application::_initRenderers(std::string& failed)
{
    _renderers["VulkanRenderer"] = std::make_unique<VulkanRenderer>(*_window, _vulkan);
    _rendererNames.push_back("NavigationRenderer");
    _activeRenderer = 0;

    for (std::pair<const std::string, std::unique_ptr<Renderer>>& pair : _renderers) {
        std::unique_ptr<Renderer>& renderer = pair.second;
        renderer->setCamera(_camera);
        renderer->setScene(_scene);
    }

    for (std::pair<const std::string, std::unique_ptr<Renderer>>& pair : _renderers) {
        std::unique_ptr<Renderer>& renderer = pair.second;
        if (renderer->init()) {
            failed = pair.first;
            break;
        }
    }

    return failed.size() ? -1 : 0;
}

void Application::_cleanUp()
{
    _vulkan.waitForIdleDevice();

    std::string failed;
    for (std::pair<const std::string, std::unique_ptr<VulkanRenderer>>& pair : _renderers) {
        std::unique_ptr<VulkanRenderer>& renderer = pair.second;
        if (renderer->cleanup()) {
            failed = pair.first;
            break;
        }
    }

    if (failed.size()) {
        std::cerr << "Failed to cleanup renderer " << failed << std::endl;
    }

    _renderers.clear();

    _vulkan.cleanup();
}

int Application::loadScene(const std::string& filePath)
{
    /*
    _scene = SceneFactory::createScene();

    TransformParameters t;
    if (!ModelLoader::loadModel(filePath, *_scene, Transform(t))) {
        std::cerr << "Could not load model " << filePath << std::endl;
        return -1;
    }
    */

    return 0;
}

int Application::loadScene(Scene* scene)
{
    _scene = scene;
    return 0;
}

void Application::setCamera(const Camera& camera)
{
    _camera = camera;
}

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
