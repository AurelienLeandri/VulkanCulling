#pragma once

struct GLFWwindow;

class Window
{
public:
	enum class Context
	{
		NONE = 0,
		OPEN_GL = 1,
	};

public:
	Window(size_t width, size_t height);
	~Window();

	Window(const Window& other) = delete;
	Window(const Window&& other) = delete;
	Window& operator=(const Window& other) = delete;
	Window& operator=(const Window&& other) = delete;

public:
	int init(Context context = Context::NONE);

public:
	GLFWwindow* window = nullptr;

	// TODO: should be changed when resizing. This is not used right now but it might be dangerous later.
	size_t width;
	size_t height;
};

