#include "OpenGLError.h"

OpenGLRendererException::OpenGLRendererException(const char* message) :
	_message(message)
{
}

OpenGLRendererException::OpenGLRendererException(const char* message) :
	_message(message)
{
}

const char* OpenGLRendererException::what() const noexcept
{
	return _message;
}
