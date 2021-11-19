#version 460

vec3 positions[3] = vec3[](
    vec3(0.0, -0.5, 0.0),
    vec3(0.5, 0.5, 0.0),
    vec3(-0.5, 0.5, 0.0)
);

vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoord;

layout (location = 0) out vec3 fragNormal;
layout (location = 1) out vec2 fragTexCoord;

layout(set = 0, binding = 0) uniform UniformBufferObject {
	mat4 view;
    mat4 proj;
    mat4 viewProj;
} camera;

struct ObjectData{
	mat4 model;
};

layout(std140, set = 1, binding = 0) readonly buffer ObjectBuffer{
	ObjectData objects[];
} objectBuffer;

void main() {
	gl_Position = camera.viewProj * objectBuffer.objects[gl_InstanceIndex].model * vec4(inPosition, 1.0);
	// gl_Position = vec4(inPosition, 1.0);
    // fragNormal = normalize(vec3(transpose(inverse(camera.model)) * vec4(inNormal, 0.0)));
    fragNormal = inNormal;
    fragTexCoord = vec2(inTexCoord.x, 1.0 - inTexCoord.y);
}