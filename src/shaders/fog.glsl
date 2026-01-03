#ifndef FOG_FUNCTIONS_GLSL
#define FOG_FUNCTIONS_GLSL

#extension GL_GOOGLE_include_directive : require
#include "fog_settings.h"

float getFogFactorLinear(const vec3 frag_pos, const vec3 view_pos, const FogSettings fog)
{
    float d = distance(frag_pos, view_pos);
    float fog_range = fog.fog_end - fog.fog_start;
    float fog_dist = fog.fog_end - d;
    float fog_factor = fog_dist / fog_range;
    fog_factor = clamp(fog_factor, 0.f, 1.f);
    return fog_factor;
}

float getFogFactorExp(const vec3 frag_pos, const vec3 view_pos, const FogSettings fog)
{
    float d = length(frag_pos - view_pos);
    float dist_ratio = 4.f * d / fog.fog_end;
    float fog_factor = exp(-dist_ratio * fog.fog_density);
    return fog_factor;
}

float getFogFactorExpSq(const vec3 frag_pos, const vec3 view_pos, const FogSettings fog)
{
    float d = length(frag_pos - view_pos);
    float dist_ratio = 4.f * d / fog.fog_end;
    float f = dist_ratio * fog.fog_density;
    float fog_factor = exp(-f * f);
    return fog_factor;
}

float getFogFactor(const vec3 frag_pos, const vec3 view_pos, const FogSettings fog) {
    float maskNone = float(fog.fog_type == FOG_NONE);
    float maskLinear = float(fog.fog_type == FOG_LINEAR);
    float maskExp = float(fog.fog_type == FOG_EXP);
    float maskExpSq = float(fog.fog_type == FOG_EXP_SQ);

    return maskNone * 1.f
        + maskLinear * getFogFactorLinear(frag_pos, view_pos, fog)
        + maskExp * getFogFactorExp(frag_pos, view_pos, fog)
        + maskExpSq * getFogFactorExpSq(frag_pos, view_pos, fog);
}

#endif // !FOG_FUNCTIONS_GLSL
