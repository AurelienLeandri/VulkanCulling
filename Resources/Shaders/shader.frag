#version 450

layout (location = 0) in vec3 fragNormal;
layout (location = 1) in vec2 fragTexCoord;

layout (location = 0) out vec4 outColor;

layout (set = 1, binding = 0) uniform sampler2D diffuseSampler;
layout (set = 1, binding = 1) uniform sampler2D specularSampler;
layout (set = 1, binding = 2) uniform sampler2D ambientSampler;
layout (set = 1, binding = 3) uniform sampler2D normalSampler;
layout (set = 1, binding = 4) uniform sampler2D heightSampler;

void main() {
	//outColor = texture(diffuseSampler, fragTexCoord);
	outColor = vec4(fragTexCoord, 0, 1);
}