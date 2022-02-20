#pragma once

#include <glad/glad.h>

#include <exception>
#include <iostream>
#include <unordered_map>

void CheckOpenGLError(const char* file, int line);

#ifdef _DEBUG
#define GL_CHECK(function_call)                                     \
	do                                                              \
	{                                                               \
        function_call;                                              \
        CheckOpenGLError(__FILE__, __LINE__);                       \
	} while (0)
#else
#define GL_CHECK(function_call)                                     \
	do                                                              \
	{                                                               \
        function_call;                                              \
	} while (0)
#endif

class OpenGLRendererException : public std::exception {
public:
	OpenGLRendererException(const char* message = nullptr);
	virtual const char* what() const noexcept;
private:
	const char* _message = nullptr;
};