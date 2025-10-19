#version 450 core

#extension GL_GOOGLE_include_directive : require
#include "scene_data.glsl"
#include "material_features.h"

layout(location = 0) out vec4 OutFragColor;
layout(location = 0) in vec3 inFragPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColor;
layout(location = 3) in vec2 inUv;
// layout(location = 3) in vec3 inViewPos;

layout(set = 2, binding = 0) uniform sampler2D TexDiffuse;
layout(set = 2, binding = 1) uniform sampler2D TexMetalRough;
layout(set = 2, binding = 2) uniform sampler2D TexNormal;
layout(set = 2, binding = 3) uniform sampler2D TexAO;

// Data global to whole scene (120 bytes data, 8 to pad)
layout(std140, set = 3, binding = 0) uniform uSceneData {
    SceneData scene;
};

// Material-specific data. TODO: add normal scaling and AO strength params
layout(set = 3, binding = 1) uniform uMaterialData {
    vec4 color_factors;
    float metal_factor;
    float rough_factor;
    uint feature_flags;
    float _pad[1];
} mat;
// layout(std140, set = 1, binding = 1) uniform uDrawData {
//     mat4 mat_m;
//     uint mat_idx;
// };

const float PI = 3.14159265359;

struct {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    // vec3 specular;
} SunLight = {
        sun_dir.xyz,
        vec3(sun_color.w),
        sun_color.xyz,
    };

float DistributionGGX(vec3 normal, vec3 hvec, float roughness);
float GeometrySchlickGGX(float NdotV, float roughness);
float GeometrySmith(vec3 normal, vec3 view_dir, vec3 light_dir, float roughness);
vec3 FresnelSchlick(float cosTheta, vec3 F0);

void main()
{
    vec3 normal = normalize(inNormal);
    vec3 view_dir = normalize(camera_world.xyz - inFragPos);

    vec4 diffuse_color = mat.color_factors;
    float metalness = mat.metal_factor;
    float roughness = mat.metal_factor;
    float ao = 1.0;
    vec3 F0 = vec3(0.04); // default for dielectrics, updated if material has metalness

    // ** FEATURE TEST ** //
    if (bool(mat.feature_flags & HAS_DIFFUSE_TEX)) {
        diffuse_color *= texture(TexDiffuse, inUv);
        // diffuse_color.rgb = pow(diffuse_color.rgb, vec3(2.2));
    } 
    diffuse_color *= inColor;

    if (bool(mat.feature_flags & HAS_METALROUGH_TEX)) {
        vec2 metalrough = texture(TexMetalRough, inUv).gb;
        metalness = metalrough.x * mat.metal_factor;
        roughness = metalrough.y * mat.rough_factor;
    }

    if (bool(mat.feature_flags & HAS_OCCLUSION_TEX)) {
        ao = texture(TexAO, inUv).r;
    }

    // TODO: normal & emissive maps

    // ** COMPUTE LOOP ** //
    vec3 Lo = vec3(0.0); // accumulated result from all lights. TODO: vec4 for alpha
    for (int i = 0; i < 1; i++) {
        vec3 light_dir = normalize(SunLight.direction);
        vec3 hvec = normalize(view_dir + light_dir);

        // float distance = length(SunLight.direction);
        // float attenuation = 1.0 / (distance * distance);
        vec3 radiance = SunLight.diffuse /* * attenuation */; // skip attenuation because directional light

        float D = DistributionGGX(normal, hvec, roughness);
        float G = GeometrySmith(normal, view_dir, light_dir, roughness);
        vec3 F = FresnelSchlick(max(dot(hvec, view_dir), 0.0), F0);

        vec3 ks = F;
        vec3 kd = vec3(1.0) - ks;
        kd *= 1.0 - metalness; // nullify diffuse if material is metallic. metals only have specular

        float ndotl = max(dot(normal, light_dir), 0.0);
        vec3 DFG = D * F * G;
        float den = 4.0 * max(dot(normal, view_dir), 0.0) * ndotl;
        vec3 specular = DFG / (den + 0.0001);

        // TODO: use diffuse color's alpha
        Lo += (kd * diffuse_color.rgb / PI + specular) * radiance * ndotl;
        Lo += specular;
    }

    // ambient
    float ao_strength = 1.0; // TODO: use strength param from model
    ao = 1.0 + ao_strength * (ao - 1.0);
    vec3 ambient = vec3(0.03) * diffuse_color.rgb * ao;
    vec3 result = Lo; 

    // tone mapping + gamma correction
    result = result / (result + vec3(1.0));
    result = pow(result, vec3(1.0 / 2.2));

    OutFragColor = vec4(result, 1.0f);
}

float DistributionGGX(vec3 normal, vec3 hvec, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(normal, hvec), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float den = (NdotH2 * (a2 - 1.0) + 1.0);
    den = PI * den * den;

    return num / den;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float den = NdotV * (1.0 - k) + k;

    return num / den;
}

float GeometrySmith(vec3 normal, vec3 view_dir, vec3 light_dir, float roughness) {
    float NdotV = max(dot(normal, view_dir), 0.0);
    float NdotL = max(dot(normal, light_dir), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
