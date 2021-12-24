#version 460

layout (location = 0) in vec3 fragNormal;
layout (location = 1) in vec2 fragTexCoord;
layout (location = 2) in vec3 fragCoord;

layout (location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform SceneData {   
	vec4 ambientColor;
	vec4 sunlightDirection; //w for sun power
	vec4 sunlightColor;
} sceneData;

// Material
layout(set = 2, binding = 0) uniform sampler2D diffuseTexture;
layout(set = 2, binding = 1) uniform sampler2D specularTexture;
layout(set = 2, binding = 2) uniform sampler2D ambientTexture;
layout(set = 2, binding = 3) uniform sampler2D normalTexture;
layout(set = 2, binding = 4) uniform sampler2D heightTexture;

void main() {
	outColor = texture(diffuseTexture, fragTexCoord);
	//outColor = vec4(fragTexCoord, gl_FragCoord.z, 1);
	//outColor = vec4(vec3(1.0) - fragCoord / 250.0, 1);
	//outColor = vec4(fragNormal, 1.0);
}