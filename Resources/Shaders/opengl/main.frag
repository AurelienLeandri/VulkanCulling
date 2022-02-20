#version 460 core

in vec3 worldPosition;
in vec3 worldNormal;
in vec2 uv;

out vec4 FragColor;  

// Material
uniform sampler2D diffuseTexture;
uniform sampler2D specularTexture;
uniform sampler2D ambientTexture;
uniform sampler2D normalTexture;
uniform sampler2D heightTexture;

void main()
{
    FragColor = texture2D(diffuseTexture, uv);
}