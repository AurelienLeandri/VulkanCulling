#include "InputManager.h"

#include "Application.h"
#include "Window.h"

#include <Scene/Camera.h>

bool InputManager::framebufferResized = false;
const float InputManager::_MOVEMENT_SPEED = 5.f;

InputManager::InputManager(Application& application) :
    _application(application)
{
}

void InputManager::init()
{
    _camera = _application._camera.get();
    _window = _application._window->window;
    glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetWindowUserPointer(_window, &_application);
    glfwSetFramebufferSizeCallback(_window, _framebufferResizeCallback);
    glfwSetCursorPosCallback(_window, _mouseCallback);

    _frameClock = std::clock();
}

bool InputManager::processInput()
{
    glfwPollEvents();

    // Keyboard
    float deltaTime = (float(std::clock()) - _frameClock) / (float)CLOCKS_PER_SEC;
    _frameClock = std::clock();
    if (glfwGetKey(_window, GLFW_KEY_W) == GLFW_PRESS)
        _processKeyboard(CameraMovement::FORWARD, deltaTime);
    if (glfwGetKey(_window, GLFW_KEY_S) == GLFW_PRESS)
        _processKeyboard(CameraMovement::BACKWARD, deltaTime);
    if (glfwGetKey(_window, GLFW_KEY_A) == GLFW_PRESS)
        _processKeyboard(CameraMovement::LEFT, deltaTime);
    if (glfwGetKey(_window, GLFW_KEY_D) == GLFW_PRESS)
        _processKeyboard(CameraMovement::RIGHT, deltaTime);
    if (glfwGetKey(_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        _processKeyboard(CameraMovement::DOWN, deltaTime);
    if (glfwGetKey(_window, GLFW_KEY_SPACE) == GLFW_PRESS)
        _processKeyboard(CameraMovement::UP, deltaTime);
    return !(glfwGetKey(_window, GLFW_KEY_ESCAPE) == GLFW_PRESS || glfwWindowShouldClose(_window));

    // Closing window
    return glfwWindowShouldClose(_window);
}

void InputManager::processMouseMovement(float xoffset, float yoffset)
{
    static const float MOUSE_SENSITIVITY = 0.1f;
    xoffset *= MOUSE_SENSITIVITY;
    yoffset *= MOUSE_SENSITIVITY;

    _currentYaw += xoffset;
    _currentPitch += yoffset;

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

void InputManager::_processKeyboard(CameraMovement direction, float deltaTime)
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

void InputManager::_framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    framebufferResized = true;
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

    Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    app->_inputManager->processMouseMovement(xoffset, yoffset);
}
