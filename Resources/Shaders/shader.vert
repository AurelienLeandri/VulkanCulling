#version 450

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoord;

layout (location = 0) out vec3 fragNormal;
layout (location = 1) out vec2 fragTexCoord;

layout (push_constant) uniform push_constant_t {
	mat4 view;
} push_constants;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 proj;
} transforms;

void main() {
	gl_Position = transforms.proj * push_constants.view * transforms.model * vec4(inPosition, 1.0);
    fragNormal = inNormal;
    fragTexCoord = vec2(inTexCoord.x, 1.0 - inTexCoord.y);
}