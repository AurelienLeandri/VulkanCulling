#version 430

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoord;

layout (location = 0) out vec3 fragNormal;
layout (location = 1) out vec2 fragTexCoord;
layout (location = 2) out vec3 fragCoord;

layout(set = 0, binding = 0) uniform UniformBufferObject {
	mat4 view;
    mat4 proj;
    mat4 viewProj;
    mat4 invProj;
} camera;

layout (set = 0, binding = 2) buffer IndexMap {
	uint map[];
} objectDataIndices;

struct ObjectData{
	mat4 model;
	vec4 sphereBounds;
};

layout(set = 1, binding = 0) readonly buffer ObjectBuffer{
	ObjectData objects[];
} objectBuffer;

void main() {
	uint dataIndex = objectDataIndices.map[gl_InstanceIndex];
	gl_Position = camera.viewProj * objectBuffer.objects[dataIndex].model * vec4(inPosition, 1.0);
    fragNormal = inNormal;  // NOTE: Not used for now
	fragTexCoord = vec2(inTexCoord.x, 1.0 - inTexCoord.y);
	fragCoord = vec3(objectBuffer.objects[dataIndex].model * vec4(inPosition, 1.0));
}