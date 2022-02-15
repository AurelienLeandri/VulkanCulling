#pragma once

#include <string>
#include <glm/glm.hpp>

class Shader
{
public:
    Shader(const char* vertexPath, const char* fragmentPath);

    void use();

    void setBool(const std::string& uniformName, bool value) const;
    void setInt(const std::string& uniformName, int value) const;
    void setFloat(const std::string& uniformName, float value) const;
    void setMat(const std::string& uniformName, const glm::mat4& mat) const;

private:
    unsigned int _programID = 0;
};