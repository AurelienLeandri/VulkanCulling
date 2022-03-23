#include "InputManager.h"

#include "Window.h"
#include "Application.h"
#include "ApplicationState.h"

#define GLFW_INCLUDE_VULKAN
#include <glfw/glfw3.h>

#include <scene/Camera.h>

const float InputManager::_MOVEMENT_SPEED = 5.f;

InputManager::InputManager()
{
}

void InputManager::init(Window* window, Application* application, ApplicationState* applicationState)
{
    _application = application;
    _window = window;
    _applicationState = applicationState;
    glfwSetInputMode(_window->window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetWindowUserPointer(_window->window, this);
    glfwSetCursorPosCallback(_window->window, _mouseCallback);
    glfwSetFramebufferSizeCallback(_window->window, _framebufferSizeCallback);

    _frameClock = std::clock();
}

void InputManager::setCamera(leoscene::Camera* camera)
{
    _camera = camera;
}

bool InputManager::processInput()
{
    glfwPollEvents();

    float deltaTime = (float(std::clock()) - _frameClock) / (float)CLOCKS_PER_SEC;
    _frameClock = std::clock();

    // Keys for camera update
    if (glfwGetKey(_window->window, GLFW_KEY_W) == GLFW_PRESS)
        _updateApplicationCamera(CameraMovement::FORWARD, deltaTime);
    if (glfwGetKey(_window->window, GLFW_KEY_S) == GLFW_PRESS)
        _updateApplicationCamera(CameraMovement::BACKWARD, deltaTime);
    if (glfwGetKey(_window->window, GLFW_KEY_A) == GLFW_PRESS)
        _updateApplicationCamera(CameraMovement::LEFT, deltaTime);
    if (glfwGetKey(_window->window, GLFW_KEY_D) == GLFW_PRESS)
        _updateApplicationCamera(CameraMovement::RIGHT, deltaTime);
    if (glfwGetKey(_window->window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        _updateApplicationCamera(CameraMovement::DOWN, deltaTime);
    if (glfwGetKey(_window->window, GLFW_KEY_SPACE) == GLFW_PRESS)
        _updateApplicationCamera(CameraMovement::UP, deltaTime);

    // Keys for application state update
    if (glfwGetKey(_window->window, GLFW_KEY_O) == GLFW_PRESS && !_oPressed)
        _oPressed = true;
    else if (glfwGetKey(_window->window, GLFW_KEY_O) == GLFW_RELEASE && _oPressed) {
        _oPressed = false;
        _updateApplicationState(ApplicationToggle::OCCLUSION_CULLING);
    }

    if (glfwGetKey(_window->window, GLFW_KEY_F) == GLFW_PRESS && !_fPressed)
        _fPressed = true;
    else if (glfwGetKey(_window->window, GLFW_KEY_F) == GLFW_RELEASE && _fPressed) {
        _fPressed = false;
        _updateApplicationState(ApplicationToggle::FRUSTUM_CULLING);
    }

    if (glfwGetKey(_window->window, GLFW_KEY_T) == GLFW_PRESS && !_tPressed)
        _tPressed = true;
    else if (glfwGetKey(_window->window, GLFW_KEY_T) == GLFW_RELEASE && _tPressed) {
        _tPressed = false;
        _updateApplicationState(ApplicationToggle::MAKE_ALL_OBJECTS_TRANSPARENT);
    }

    if (glfwGetKey(_window->window, GLFW_KEY_L) == GLFW_PRESS && !_lPressed)
        _lPressed = true;
    else if (glfwGetKey(_window->window, GLFW_KEY_L) == GLFW_RELEASE && _lPressed) {
        _lPressed = false;
        _updateApplicationState(ApplicationToggle::LOCK_FRUSTUM_CULLING_CAMERA);
    }

    // Closing window if needed
    return !(glfwGetKey(_window->window, GLFW_KEY_ESCAPE) == GLFW_PRESS || glfwWindowShouldClose(_window->window));
}

void InputManager::processMouseMovement(float xoffset, float yoffset)
{
    static const float MOUSE_SENSITIVITY = 0.1f;
    xoffset *= MOUSE_SENSITIVITY;
    yoffset *= MOUSE_SENSITIVITY;

    float yaw = _applicationState->fpsCamera.yaw - xoffset;
    if (yaw > 360.0f) yaw -= 360.0f;

    float pitch = glm::clamp(_applicationState->fpsCamera.pitch + yoffset, -89.0f, 89.0f);

    _applicationState->fpsCamera.yaw = yaw;
    _applicationState->fpsCamera.pitch = pitch;

    float radPitch = glm::radians(pitch);
    float radYaw = glm::radians(yaw);
    float cosPitch = glm::cos(radPitch);
    float sinPitch = glm::sin(radPitch);
    float cosYaw = glm::cos(radYaw);
    float sinYaw = glm::sin(radYaw);

    glm::vec3 xAxis{ cosYaw, 0, -sinYaw };
    glm::vec3 yAxis{ sinYaw * sinPitch, cosPitch, cosYaw * sinPitch };
    glm::vec3 zAxis{ sinYaw * cosPitch, -sinPitch, cosPitch * cosYaw };

    glm::vec3 cameraFront = glm::normalize(glm::vec3(
        glm::cos(radYaw) * cosPitch,
        glm::sin(radPitch),
        glm::sin(radYaw) * cosPitch
    ));

    _camera->setFront(cameraFront);
}

void InputManager::_updateApplicationCamera(CameraMovement direction, float deltaTime)
{
    float velocity = _MOVEMENT_SPEED * deltaTime;
    const glm::vec3& cameraFront = _camera->getFront();
    const glm::vec3& cameraRight = _camera->getRight();
    switch (direction) {
    case CameraMovement::FORWARD:
        _camera->setPosition(_camera->getPosition() + glm::vec3(cameraFront.x, 0, cameraFront.z) * velocity);
        _applicationState->fpsCamera.position.z += velocity;
        break;
    case CameraMovement::BACKWARD:
        _camera->setPosition(_camera->getPosition() - glm::vec3(cameraFront.x, 0, cameraFront.z) * velocity);
        _applicationState->fpsCamera.position.z -= velocity;
        break;
    case CameraMovement::LEFT:
        _camera->setPosition(_camera->getPosition() - glm::vec3(cameraRight.x, 0, cameraRight.z) * velocity);
        _applicationState->fpsCamera.position.x -= velocity;
        break;
    case CameraMovement::RIGHT:
        _camera->setPosition(_camera->getPosition() + glm::vec3(cameraRight.x, 0, cameraRight.z) * velocity);
        _applicationState->fpsCamera.position.x += velocity;
        break;
    case CameraMovement::UP:
        _camera->setPosition(_camera->getPosition() - glm::vec3(0, velocity, 0));
        _applicationState->fpsCamera.position.y -= velocity;
        break;
    case CameraMovement::DOWN:
        _camera->setPosition(_camera->getPosition() + glm::vec3(0, velocity, 0));
        _applicationState->fpsCamera.position.y += velocity;
        break;
    }
}

void InputManager::_updateApplicationState(ApplicationToggle toggle)
{
    switch (toggle) {
    case ApplicationToggle::OCCLUSION_CULLING:
        _applicationState->occlusionCulling = !_applicationState->occlusionCulling;
        break;
    case ApplicationToggle::FRUSTUM_CULLING:
        _applicationState->frustumCulling = !_applicationState->frustumCulling;
        break;
    case ApplicationToggle::MAKE_ALL_OBJECTS_TRANSPARENT:
        _applicationState->makeAllObjectsTransparent = !_applicationState->makeAllObjectsTransparent;
        break;
    case ApplicationToggle::LOCK_FRUSTUM_CULLING_CAMERA:
        _applicationState->lockCullingCamera = !_applicationState->lockCullingCamera;
        break;
    }
}


void InputManager::_mouseCallback(GLFWwindow* window, double xpos, double ypos)
{
    static bool firstTime = true;
    static float lastX = 0, lastY = 0;
    float xposf = static_cast<float>(xpos);
    float yposf = static_cast<float>(ypos);
    if (firstTime)
    {
        lastX = xposf;
        lastY = yposf;
        firstTime = false;
    }

    float xoffset = xposf - lastX;
    float yoffset = lastY - yposf; // reversed since y-coordinates go from bottom to top

    lastX = xposf;
    lastY = yposf;

    InputManager* inputManager = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
    inputManager->processMouseMovement(xoffset, yoffset);
}

void InputManager::_framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    InputManager* inputManager = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
    inputManager->_window->width = static_cast<size_t>(width);
    inputManager->_window->height = static_cast<size_t>(height);
    inputManager->_application->notifyWindowResize();
}

