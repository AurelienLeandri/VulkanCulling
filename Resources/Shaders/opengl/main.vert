#version 460 core

layout (location = 0) in vec3 position;

layout(std430, binding = 3) buffer ObjectData
{
    mat4 modelTransforms[];
} objectData;

uniform mat4 view;
uniform mat4 proj;
uniform mat4 viewProj;

void main()
{
    gl_Position = viewProj * objectData.modelTransforms[gl_InstanceID] * vec4(position, 1.0);
}   