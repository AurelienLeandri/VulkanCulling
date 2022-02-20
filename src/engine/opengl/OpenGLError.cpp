#include "OpenGLError.h"

#include <string>

OpenGLRendererException::OpenGLRendererException(const char* message) :
	_message(message)
{
}

const char* OpenGLRendererException::what() const noexcept
{
	return _message;
}

void CheckOpenGLError(const char* file, int line)
{
    static const std::unordered_map<GLenum, std::string> errorCodeToString{
           {GL_INVALID_ENUM, "GL_INVALID_ENUM"},
           {GL_INVALID_VALUE, "GL_INVALID_VALUE"},
           {GL_INVALID_OPERATION, "GL_INVALID_OPERATION"},
           {GL_STACK_OVERFLOW, "GL_STACK_OVERFLOW"},
           {GL_STACK_UNDERFLOW, "GL_STACK_UNDERFLOW"},
           {GL_OUT_OF_MEMORY, "GL_OUT_OF_MEMORY"},
           {GL_INVALID_FRAMEBUFFER_OPERATION, "GL_INVALID_FRAMEBUFFER_OPERATION"},
    };

    GLenum errorCode = glGetError();
    if (!errorCode) { return; }
    while (errorCode)
    {
        try {
            std::cerr << "Error (notification from glGetError): " << errorCodeToString.at(errorCode)
                << " in file \"" << file << "\", line " << line << "." << std::endl;
        }
        catch (const std::out_of_range&) {
            std::cerr << "Error (notification from glGetError): " << "OTHER (code: 0x" << std::hex << errorCode << std::dec << ")"
                << " in file \"" << file << "\", line " << line << "." << std::endl;
        }
        errorCode = glGetError();
    }
    throw OpenGLRendererException("Error: Notified by glGetError while using the OpenGL API.");
}