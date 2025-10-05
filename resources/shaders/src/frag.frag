#version 450 core

#extension GL_GOOGLE_include_directive : require
#include "scene_data.glsl"

layout(location = 0) out vec4 OutFragColor;
layout(location = 0) in vec2 inUv;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inFragPos;
// layout(location = 3) in vec3 inViewPos;

layout(set = 2, binding = 0) uniform sampler2D TexDiffuse;
layout(set = 2, binding = 1) uniform sampler2D TexMetalRough;
layout(set = 2, binding = 2) uniform sampler2D TexNormal;

// Data global to whole scene (120 bytes data, 8 to pad)
layout(std140, set = 3, binding = 0) uniform uSceneData {
    SceneData scene;
};

// Material-specific data
layout(set = 3, binding = 1) uniform uMaterialData {
    vec4 color_factors;
    vec4 metal_rough_factors;
};

struct {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    // vec3 specular;
} SunLight = {
        sun_dir.xyz,
        vec3(sun_dir.w),
        sun_color.xyz,
    };

void main()
{
    vec3 norm = normalize(inNormal);
    vec3 viewDir = normalize(camera_world.xyz - inFragPos);

    // ambient
    vec3 ambient = SunLight.ambient * texture(TexDiffuse, inUv).rgb;

    // diffuse
    vec3 lightDir = normalize(SunLight.direction);
    float diff_fact = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = SunLight.diffuse * diff_fact * texture(TexDiffuse, inUv).rgb;

    vec3 result = diffuse;

    OutFragColor = vec4(result, 1.0f);
}
