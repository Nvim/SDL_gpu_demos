#version 450 core

#extension GL_GOOGLE_include_directive : require
#include "fog_settings.h"
#include "fog.glsl"

layout(location = 0) out vec4 OutFragColor;
layout(location = 0) in vec3 inViewPos;
layout(location = 1) in vec3 inFragPos;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in float inLocalHeight;

struct DirLight {
    vec3 direction;
    float pad_1;
    vec3 ambient;
    float pad_2;
    vec3 diffuse;
    float pad_3;
    vec3 specular;
};

layout(std140, set = 3, binding = 0) uniform uDirLight {
    DirLight sun;
};

layout(std140, set = 3, binding = 1) uniform uColor {
    vec3 grass_color;
};

layout(std140, set = 3, binding = 2) uniform uFog {
    FogSettings fog;
};

void main()
{
    vec3 normal = normalize(inNormal);

    float den = 1.5f;
    float halfden = (1.f / 1.5f) / 2.f;
    float y = inLocalHeight / den; // [0,1] -> [0,0.5]
    float color_fact = y - halfden; // [0,0.5] -> [-0.25,0.25]

    vec3 color = grass_color + color_fact;

    float fog_fact = 1.f - getFogFactor(inFragPos, inViewPos, fog);
    color = mix(color, fog.fog_color, fog_fact);
    OutFragColor = vec4(color, 1.f);

    // vec3 ambient = sun.ambient * color;
    //
    // vec3 light_dir = normalize(sun.direction);
    // float diffuse_factor = max(dot(normal, light_dir), 0.f);
    // vec3 diffuse = sun.diffuse * diffuse_factor * color;
    //
    // vec3 view_dir = normalize(inViewPos - inFragPos);
    // vec3 specular_dir = reflect(-light_dir, normal);
    // float specular_factor = pow(max(dot(view_dir, specular_dir), 0.f), 32);
    // vec3 specular = sun.specular * specular_factor * color;
    //
    // vec3 result = ambient + diffuse + specular;
}
