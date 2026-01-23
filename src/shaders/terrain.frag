#version 450 core

#extension GL_GOOGLE_include_directive : require
#include "fog_settings.h"
#include "fog.glsl"

layout(location = 0) out vec4 OutFragColor;

layout(location = 0) in vec3 inViewPos;
layout(location = 1) in vec3 inFragPos;
layout(location = 2) in vec3 inColor;

layout(std140, set = 3, binding = 0) uniform uFog {
    FogSettings fog;
};

void main()
{
    float fog_fact = 1.f - getFogFactor(inFragPos, inViewPos, fog);
    vec3 color = mix(inColor, fog.fog_color, fog_fact);
    OutFragColor = vec4(color, 1.f);
    // OutFragColor = vec4(inColor, 1.f);
}
