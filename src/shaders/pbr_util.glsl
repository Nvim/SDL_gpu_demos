#include "material_features.h"
#include "pbr_flags.h"

struct PointLight {
    vec3 world_pos;
    vec3 color;
};
const float PI = 3.14159265359;

// ************************** MATERIAL ************************************* //
// Material struct sent from app
struct MaterialUniform {
    vec4 color_factors; // rgba diffuse color
    vec4 other_factors; // metal - rough - normal - occlusion
    vec3 emissive_factor;
    uint feature_flags;
};

// Result of all Material_XXX functions below:
struct MaterialPBRData {
    vec4 diffuse;
    vec3 normal;
    vec3 emissive;
    float metalness;
    float roughness;
    float ao;
};

#define DEBUG_FLAG(value) bool(flags & value)
#define MATERIAL_FLAG(value) bool(mat.feature_flags & value)
vec4 Material_GetDiffuse(MaterialUniform mat, vec4 vertex_color, sampler2D tex, vec2 uv, uint flags) {
    vec4 diffuse_color = mat.color_factors;
    if (MATERIAL_FLAG(HAS_DIFFUSE_TEX) && DEBUG_FLAG(USE_DIFFUSE_TEX)) {
        diffuse_color *= texture(tex, uv);
    }
    if (DEBUG_FLAG(USE_VERTEX_COLOR)) {
        diffuse_color *= vertex_color;
    }
    return diffuse_color;
}

vec3 Material_GetNormal(MaterialUniform mat, vec3 vertex_normal, mat3 tbn, sampler2D tex, vec2 uv, uint flags) {
    vec3 normal = normalize(vertex_normal);
    if (MATERIAL_FLAG(HAS_NORMAL_TEX) && DEBUG_FLAG(USE_NORMAL_TEX)) {
        normal = texture(tex, uv).rgb;
        normal = normal * 2.0 - 1.0; // [0, 1] -> [-1, 1]
        normal = normalize(tbn * normal); // tangent -> world
        if (!gl_FrontFacing) {
            normal *= -1.0f;
        }
    }
    if (DEBUG_FLAG(USE_NORMAL_FACT)) {
        normal *= vec3(mat.other_factors.z, mat.other_factors.z, 1.0);
    }
    return normal;
}

vec2 Material_GetMetalRough(MaterialUniform mat, sampler2D tex, vec2 uv, uint flags) {
    float metalness = mat.other_factors.r;
    float roughness = mat.color_factors.g;

    if (MATERIAL_FLAG(HAS_METALROUGH_TEX)) {
        vec3 metalrough = texture(tex, uv).rgb;
        metalness *= metalrough.b;
        roughness *= metalrough.g;
        roughness = clamp(roughness, 0.04, 1.0);
        roughness = roughness * roughness;
    }
    return vec2(
        DEBUG_FLAG(USE_METAL_TEX) ? metalness : 0.0,
        DEBUG_FLAG(USE_ROUGH_TEX) ? roughness : 0.0
    );
}

vec3 Material_GetEmissive(MaterialUniform mat, sampler2D tex, vec2 uv, uint flags) {
    vec3 emissive = vec3(0.0);
    if (MATERIAL_FLAG(HAS_EMISSIVE_TEX) && DEBUG_FLAG(USE_EMISSIVE_TEX)) {
        emissive = texture(tex, uv).rgb;
    }
    if (DEBUG_FLAG(USE_EMISSIVE_FACT)) {
        emissive *= mat.emissive_factor;
    }
    return emissive;
}

float Material_GetAO(MaterialUniform mat, sampler2D tex, vec2 uv, uint flags) {
    float ao = 1.0;
    if (MATERIAL_FLAG(HAS_OCCLUSION_TEX) && DEBUG_FLAG(USE_OCCLUSION_TEX)) {
        ao = texture(tex, uv).r;
    }
    if (DEBUG_FLAG(USE_OCCLUSION_FACT)) {
        ao *= mat.other_factors.a;
    }
    return ao;
}
#undef DEBUG_FLAG
#undef MATERIAL_FLAG
// ************************************************************************* //

// ******************************* NDF ************************************* //
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

float GeometrySmith(vec3 normal, vec3 view_dir, vec3 light_direction, float roughness) {
    float NdotV = max(dot(normal, view_dir), 0.0);
    float NdotL = max(dot(normal, light_direction), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 LightContrib(
    MaterialPBRData pbr_data,
    PointLight light,
    vec3 camera_pos,
    vec3 view_dir,
    vec3 F0
) {
    vec3 light_direction = normalize(light.world_pos - camera_pos);
    vec3 hvec = normalize(view_dir + light_direction);

    float distance = length(light.world_pos - camera_pos);
    float attenuation = 1.0 / distance;
    vec3 radiance = light.color;

    // Use pbr_data instead of pbr
    float D = DistributionGGX(pbr_data.normal, hvec, pbr_data.roughness);
    float G = GeometrySmith(pbr_data.normal, view_dir, light_direction, pbr_data.roughness);
    vec3 F = FresnelSchlick(max(dot(hvec, view_dir), 0.0), F0);

    vec3 ks = F;
    vec3 kd = vec3(1.0) - ks;
    kd *= 1.0 - pbr_data.metalness; // metals have only specular

    float ndotl = max(dot(pbr_data.normal, light_direction), 0.0);
    vec3 DFG = D * F * G;
    float den = 4.0 * max(dot(pbr_data.normal, view_dir), 0.0) * ndotl;
    vec3 specular = DFG / (den + 0.0001);

    vec3 result = (kd * pbr_data.diffuse.rgb / PI + specular) * radiance * ndotl;
    return result + specular;
}
// ************************************************************************* //
