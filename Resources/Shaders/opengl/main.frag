#version 460 core

in vec4 worldPosition;

out vec4 FragColor;  

void main()
{
    FragColor = vec4((worldPosition / 100.0).xyz, 1.0);
}