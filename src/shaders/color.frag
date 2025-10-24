#version 450 core

layout (location=0) out vec4 OutFragColor;
layout (location=0) in vec3 inColor;

void main()
{
    OutFragColor = vec4(inColor, 1.0);
}
