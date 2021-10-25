#version 460

layout (location = 0) in vec3 fragNormal;
layout (location = 1) in vec2 fragTexCoord;

layout (location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform SceneData {   
	vec4 ambientColor;
	vec4 sunlightDirection; //w for sun power
	vec4 sunlightColor;
} sceneData;

void main() {
	//outColor = texture(diffuseSampler, fragTexCoord);
	outColor = vec4(fragTexCoord, 0, 1);
}