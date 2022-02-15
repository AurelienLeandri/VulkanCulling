#pragma once

#include <exception>

class OpenGLRendererException : public std::exception {
public:
	OpenGLRendererException(const char* message = nullptr);
	virtual const char* what() const noexcept;
private:
	const char* _message = nullptr;
};