#version 460

layout (location = 0) in vec3 fragNormal;
layout (location = 1) in vec2 fragTexCoord;

layout (location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform SceneData {   
	vec4 ambientColor;
	vec4 sunlightDirection; //w for sun power
	vec4 sunlightColor;
} sceneData;

layout (set = 2, binding = 0) uniform sampler2D diffuseSampler;
layout (set = 2, binding = 1) uniform sampler2D specularSampler;
layout (set = 2, binding = 2) uniform sampler2D ambientSampler;
layout (set = 2, binding = 3) uniform sampler2D normalSampler;
layout (set = 2, binding = 4) uniform sampler2D heightSampler;

void main() {
    //outColor = sceneData.ambientColor + sceneData.sunlightDirection + sceneData.sunlightColor;
	//outColor = texture(diffuseSampler, fragTexCoord);
	outColor = vec4(fragTexCoord, 0, 1);
}