#include "InputManager.h"

#include "Window.h"
#include "Application.h"

#include <scene/Camera.h>

const float InputManager::_MOVEMENT_SPEED = 5.f;

InputManager::InputManager()
{
}

void InputManager::init(GLFWwindow* window, ApplicationState* applicationState)
{
    _window = window;
    _applicationState = applicationState;
    glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetWindowUserPointer(_window, this);
    glfwSetCursorPosCallback(_window, _mouseCallback);

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
    if (glfwGetKey(_window, GLFW_KEY_W) == GLFW_PRESS)
        _updateApplicationCamera(CameraMovement::FORWARD, deltaTime);
    if (glfwGetKey(_window, GLFW_KEY_S) == GLFW_PRESS)
        _updateApplicationCamera(CameraMovement::BACKWARD, deltaTime);
    if (glfwGetKey(_window, GLFW_KEY_A) == GLFW_PRESS)
        _updateApplicationCamera(CameraMovement::LEFT, deltaTime);
    if (glfwGetKey(_window, GLFW_KEY_D) == GLFW_PRESS)
        _updateApplicationCamera(CameraMovement::RIGHT, deltaTime);
    if (glfwGetKey(_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        _updateApplicationCamera(CameraMovement::DOWN, deltaTime);
    if (glfwGetKey(_window, GLFW_KEY_SPACE) == GLFW_PRESS)
        _updateApplicationCamera(CameraMovement::UP, deltaTime);

    // Keys for application state update
    if (glfwGetKey(_window, GLFW_KEY_O) == GLFW_PRESS && !oPressed)
        oPressed = true;
    else if (glfwGetKey(_window, GLFW_KEY_O) == GLFW_RELEASE && oPressed) {
        oPressed = false;
        _updateApplicationState(ApplicationToggle::OCCLUSION_CULLING);
    }

    if (glfwGetKey(_window, GLFW_KEY_F) == GLFW_PRESS && !fPressed)
        fPressed = true;
    else if (glfwGetKey(_window, GLFW_KEY_F) == GLFW_RELEASE && fPressed) {
        fPressed = false;
        _updateApplicationState(ApplicationToggle::FRUSTUM_CULLING);
    }

    if (glfwGetKey(_window, GLFW_KEY_T) == GLFW_PRESS && !tPressed)
        tPressed = true;
    else if (glfwGetKey(_window, GLFW_KEY_T) == GLFW_RELEASE && tPressed) {
        tPressed = false;
        _updateApplicationState(ApplicationToggle::MAKE_ALL_OBJECTS_TRANSPARENT);
    }

    if (glfwGetKey(_window, GLFW_KEY_L) == GLFW_PRESS && !lPressed)
        lPressed = true;
    else if (glfwGetKey(_window, GLFW_KEY_L) == GLFW_RELEASE && lPressed) {
        lPressed = false;
        _updateApplicationState(ApplicationToggle::LOCK_FRUSTUM_CULLING_CAMERA);
    }

    // Closing window if needed
    return !(glfwGetKey(_window, GLFW_KEY_ESCAPE) == GLFW_PRESS || glfwWindowShouldClose(_window));
}

void InputManager::processMouseMovement(float xoffset, float yoffset)
{
    static const float MOUSE_SENSITIVITY = 0.1f;
    xoffset *= MOUSE_SENSITIVITY;
    yoffset *= MOUSE_SENSITIVITY;

    _currentYaw += -xoffset;
    _currentPitch += -yoffset;  // Vulkan convention

    if (_currentPitch > 89.0f)
        _currentPitch = 89.0f;
    if (_currentPitch < -89.0f)
        _currentPitch = -89.0f;

    glm::vec3 cameraFront = glm::normalize(glm::vec3(
        cos(glm::radians(_currentYaw)) * cos(glm::radians(_currentPitch)),
        -sin(glm::radians(_currentPitch)),
        sin(glm::radians(_currentYaw)) * cos(glm::radians(_currentPitch))
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
        break;
    case CameraMovement::BACKWARD:
        _camera->setPosition(_camera->getPosition() - glm::vec3(cameraFront.x, 0, cameraFront.z) * velocity);
        break;
    case CameraMovement::LEFT:
        _camera->setPosition(_camera->getPosition() - glm::vec3(cameraRight.x, 0, cameraRight.z) * velocity);
        break;
    case CameraMovement::RIGHT:
        _camera->setPosition(_camera->getPosition() + glm::vec3(cameraRight.x, 0, cameraRight.z) * velocity);
        break;
    case CameraMovement::UP:
        _camera->setPosition(_camera->getPosition() - glm::vec3(0, velocity, 0));
        break;
    case CameraMovement::DOWN:
        _camera->setPosition(_camera->getPosition() + glm::vec3(0, velocity, 0));
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
        _applicationState->lockFrustumCullingCamera = !_applicationState->lockFrustumCullingCamera;
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
