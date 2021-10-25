#version 460

layout (location = 0) in vec3 fragNormal;
layout (location = 1) in vec2 fragTexCoord;

layout (location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform SceneData {   
	vec4 ambientColor;
	vec4 sunlightDirection; //w for sun power
	vec4 sunlightColor;
} sceneData;

layout(set = 2, binding = 0) uniform sampler2D test0;
layout(set = 2, binding = 1) uniform sampler2D test1;
layout(set = 2, binding = 2) uniform sampler2D test2;
layout(set = 2, binding = 3) uniform sampler2D test3;
layout(set = 2, binding = 4) uniform sampler2D test4;

void main() {
	outColor = texture(test0, fragTexCoord);
	//outColor = vec4(fragTexCoord, 0, 1);
}