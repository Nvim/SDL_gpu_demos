#version 450 core

layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec2 uv;

layout(set = 2, binding = 0) uniform sampler2D tex;

void main()
{
    FragColor = vec4(texture(tex, uv).rgb, 1.0f);
}
