#version 330 core

layout (location = 0) in vec3 position;

uniform mat4 view;
uniform mat4 proj;
uniform mat4 viewProj;
uniform mat4 model;

void main()
{
    gl_Position = viewProj * model * vec4(position, 1.0);
}   