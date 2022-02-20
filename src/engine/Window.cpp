#include "Window.h"

#include <glfw/glfw3.h>

Window::Window(size_t width, size_t height) :
    width(width), height(height)
{
    
}

Window::~Window()
{
    if (window) {
        glfwDestroyWindow(window);
        window = nullptr;
    }

    glfwTerminate();
}

int Window::init(Context context)
{
    glfwInit();

    if (context == Context::OPEN_GL) {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef _DEBUG
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
#endif
    }
    else {  // Vulkan API
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    }

    window = glfwCreateWindow(static_cast<int>(width), static_cast<int>(height), "LeoEngine", nullptr, nullptr);

    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    return 0;
}
