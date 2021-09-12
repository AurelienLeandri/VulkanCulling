#pragma once

#define GLFW_INCLUDE_VULKAN
#include <glfw3.h>

class Window
{
public:
	Window(size_t width, size_t height);
	~Window();

	Window(const Window& other) = delete;
	Window(const Window&& other) = delete;
	Window& operator=(const Window& other) = delete;
	Window& operator=(const Window&& other) = delete;

public:
	int init();

public:
	GLFWwindow* window = nullptr;
	size_t width;
	size_t height;
};

