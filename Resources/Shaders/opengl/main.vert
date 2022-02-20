#version 460 core

layout (location = 0) in vec3 position;

layout(std430, binding = 0) readonly buffer ObjectData
{
    mat4 modelTransforms[];
} objectData;

uniform mat4 view;
uniform mat4 proj;
uniform mat4 viewProj;

out vec4 worldPosition;

void main()
{
    worldPosition = objectData.modelTransforms[gl_BaseInstance + gl_InstanceID] * vec4(position, 1.0);
    gl_Position = viewProj * worldPosition;
}   