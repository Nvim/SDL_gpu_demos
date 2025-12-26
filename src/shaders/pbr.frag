#version 450 core

#extension GL_GOOGLE_include_directive : require
#include "scene_data.glsl"
#include "pbr_util.glsl"
#include "pbr_flags.h"

layout(location = 0) out vec4 OutFragColor;
layout(location = 0) in vec3 inFragPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColor;
layout(location = 3) in vec2 inUv;
layout(location = 4) in mat3 inTBN;
// layout(location = 3) in vec3 inViewPos;

layout(set = 2, binding = 0) uniform sampler2D TexDiffuse;
layout(set = 2, binding = 1) uniform sampler2D TexMetalRough;
layout(set = 2, binding = 2) uniform sampler2D TexNormal;
layout(set = 2, binding = 3) uniform sampler2D TexAO;
layout(set = 2, binding = 4) uniform sampler2D TexEmissive;
layout(set = 2, binding = 5) uniform sampler2D TexBRDF;
layout(set = 2, binding = 6) uniform samplerCube TexIrradianceMap;
layout(set = 2, binding = 7) uniform samplerCube TexSpecularMap;

// Data global to whole scene (120 bytes data, 8 to pad)
layout(std140, set = 3, binding = 0) uniform uSceneData {
    SceneData scene;
};

// Material-specific data.
layout(std140, set = 3, binding = 1) uniform uMaterialData {
    MaterialUniform u;
} mat;

// layout(std140, set = 1, binding = 1) uniform uDrawData {
//     mat4 mat_m;
//     uint mat_idx;
// };

// Keep ibl functions here because they sample 3d maps
vec3 getIBLDiffuseLambertian(float NdotV, vec3 n, float roughness, vec3 diffuseColor, vec3 F0, vec2 brdf_sample);
vec3 getIBLRadianceContributionGGX(vec3 normal, vec3 view, vec3 specularColor, vec2 brdf_sample, float nDotV, float roughness, float specularWeight);

#define FLAG_ON(value) bool(debug_flags & value)

void main()
{
    vec2 metalRough = Material_GetMetalRough(mat.u, TexMetalRough, inUv, debug_flags);
    float metalness = FLAG_ON(USE_METAL_TEX) ? metalRough.r : 0.0;
    float roughness = FLAG_ON(USE_ROUGH_TEX) ? metalRough.g : 0.0;

    MaterialPBRData pbr_data = {
            Material_GetDiffuse(mat.u, inColor, TexDiffuse, inUv, debug_flags),
            Material_GetNormal(mat.u, inNormal, inTBN, TexNormal, inUv, debug_flags),
            Material_GetEmissive(mat.u, TexEmissive, inUv, debug_flags),
            metalness,
            roughness,
            Material_GetAO(mat.u, TexAO, inUv, debug_flags),
        };
    vec3 view_dir = normalize(camera_world - inFragPos);
    float nDotV = dot(pbr_data.normal, view_dir);
    vec3 F0 = vec3(0.04); // default for dielectrics, updated if material has metalness
    vec2 brdf_sample = texture(TexBRDF, vec2(max(nDotV, 0.0), pbr_data.roughness)).rg;
    vec3 specular_color = mix(F0, pbr_data.diffuse.rgb, pbr_data.metalness);

    PointLight light = {
            vec3(light_dir),
            vec3(light_color),
        };

    // ** COMPUTE LOOP ** //
    vec3 result = vec3(0.0);
    if (FLAG_ON(USE_POINTLIGHTS)) {
        for (int i = 0; i < 1; i++) {
            result += LightContrib(pbr_data, light, camera_world, view_dir, F0);
        }
    }

    if (FLAG_ON(USE_IBL_DIFFUSE)) {
        vec3 ibl_ambient = getIBLDiffuseLambertian(nDotV, pbr_data.normal, pbr_data.roughness, pbr_data.diffuse.rgb, F0, brdf_sample);
        result += ibl_ambient;
    }

    if (FLAG_ON(USE_IBL_SPECULAR)) {
        vec3 ibl_specular = getIBLRadianceContributionGGX(pbr_data.normal, view_dir, specular_color, brdf_sample, nDotV, pbr_data.roughness, 1.0);
        result += ibl_specular;
    }

    result += pbr_data.emissive;
    result *= pbr_data.ao;

    // Output is in Linear, HDR space
    OutFragColor = vec4(result, pbr_data.diffuse.a);
}

// ******************************* IBL ************************************* //
vec3 getIBLDiffuseLambertian(float NdotV, vec3 n, float roughness, vec3 diffuseColor, vec3 F0, vec2 brdf_sample) {
    vec2 brdfSamplePoint = clamp(vec2(NdotV, roughness), vec2(0.0, 0.0), vec2(1.0, 1.0));

    vec3 irradiance = texture(TexIrradianceMap, n.xyz).rgb;

    // see https://bruop.github.io/ibl/#single_scattering_results at Single Scattering Results
    // Roughness dependent fresnel, from Fdez-Aguera
    vec3 Fr = max(vec3(1.0 - roughness), F0) - F0;
    vec3 k_S = F0 + Fr * pow(1.0 - NdotV, 5.0);
    vec3 FssEss = k_S * brdf_sample.x + brdf_sample.y; // <--- GGX / specular light contribution (scale it down if the specularWeight is low)

    // Multiple scattering, from Fdez-Aguera
    float Ems = (1.0 - (brdf_sample.x + brdf_sample.y));
    vec3 F_avg = (F0 + (1.0 - F0) / 21.0);
    vec3 FmsEms = Ems * FssEss * F_avg / (1.0 - F_avg * Ems);
    vec3 k_D = diffuseColor * (1.0 - FssEss + FmsEms); // we use +FmsEms as indicated by the formula in the blog post (might be a typo in the implementation)

    return (FmsEms + k_D) * irradiance;
}

vec3 getIBLRadianceContributionGGX(vec3 normal, vec3 view, vec3 specularColor, vec2 brdf_sample, float nDotV, float roughness, float specularWeight) {
    vec3 reflection = -normalize(reflect(view, normal));
    float mipCount = textureQueryLevels(TexSpecularMap);
    float lod = roughness * (mipCount - 1);

    vec3 specularLight = textureLod(TexSpecularMap, reflection, lod).rgb;

    // see https://bruop.github.io/ibl/#single_scattering_results at Single Scattering Results
    // Roughness dependent fresnel, from Fdez-Aguera
    vec3 Fr = max(vec3(1.0 - roughness), specularColor) - specularColor;
    vec3 k_S = specularColor + Fr * pow(1.0 - nDotV, 5.0);
    vec3 FssEss = k_S * brdf_sample.x + brdf_sample.y;

    return specularWeight * specularLight * FssEss;
}
// ************************************************************************* //

// From https://ogldev.org/www/tutorial26/tutorial26.html
// To be used with his manual tangent computing method. No .w component
// vec3 ogldevNormal()
// {
//     vec3 Normal = normalize(inNormal);
//     vec3 Tangent = normalize(inTan);
//     Tangent = normalize(Tangent - dot(Tangent, Normal) * Normal);
//     vec3 Bitangent = cross(Tangent, Normal);
//     vec3 BumpMapNormal = texture(TexNormal, inUv).xyz;
//     BumpMapNormal = 2.0 * BumpMapNormal - vec3(1.0, 1.0, 1.0);
//     vec3 NewNormal;
//     mat3 TBN = mat3(Tangent, Bitangent, Normal);
//     NewNormal = TBN * BumpMapNormal;
//     NewNormal = normalize(NewNormal);
//     return NewNormal;
// }
//
// From LearnOpenGL Github. Not explained in tutorials.
// Works flawlessly but derivates in every fragment shader run.
// vec3 getNormalFromMap()
// {
//     vec3 tangentNormal = texture(TexNormal, inUv).xyz * 2.0 - 1.0;
//
//     vec3 Q1  = dFdx(inFragPos);
//     vec3 Q2  = dFdy(inFragPos);
//     vec2 st1 = dFdx(inUv);
//     vec2 st2 = dFdy(inUv);
//
//     vec3 N   = normalize(inNormal);
//     vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
//     vec3 B  = -normalize(cross(N, T));
//     mat3 TBN = mat3(T, B, N);
//
//     return normalize(TBN * tangentNormal);
// }
