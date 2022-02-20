#version 460 core

layout (location = 0) in vec3 iPosition;
layout (location = 1) in vec3 iNormal;
layout (location = 2) in vec3 iTangent;
layout (location = 3) in vec2 iUV;

layout(std430, binding = 0) readonly buffer ObjectData
{
    mat4 modelTransforms[];
} objectData;

uniform mat4 view;
uniform mat4 proj;
uniform mat4 viewProj;

out vec3 worldPosition;
out vec3 worldNormal;
out vec2 uv;

void main()
{
    worldPosition = (objectData.modelTransforms[gl_BaseInstance + gl_InstanceID] * vec4(iPosition, 1.0)).xyz;
    worldNormal = normalize(mat3(transpose(inverse(objectData.modelTransforms[gl_BaseInstance + gl_InstanceID]))) * iNormal);
    gl_Position = viewProj * objectData.modelTransforms[gl_BaseInstance + gl_InstanceID] * vec4(iPosition, 1.0);
    uv = vec2(iUV.x, 1.0 - iUV.y);
}   