#pragma once

#include <exception>

class OpenGLRendererException : public std::exception {
public:
	OpenGLRendererException(const char* message = nullptr);
	OpenGLRendererException(const char* message);
	virtual const char* what() const noexcept;
private:
	const char* _message = nullptr;
};