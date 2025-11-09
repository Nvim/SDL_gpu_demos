#version 450 core

layout(location = 0) in vec3 inPos;
layout(location = 0) out vec3 localPos;

layout(std140, set = 1, binding = 0) uniform uMatrices {
    mat4 projection;
    mat4 view;
};

void main()
{
localPos = inPos;
gl_Position = projection * view * vec4(localPos, 1.0);
}
