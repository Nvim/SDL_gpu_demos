#version 450 core

#extension GL_GOOGLE_include_directive : require
#include "scene_data.glsl"

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;

layout(location = 0) out vec2 outUv; // depends on vertex
layout(location = 1) out vec3 outNormal; // depends on vertex
layout(location = 2) out vec3 outFragPos; // depends on model matrix

// Data global to whole scene (120 bytes data, 8 to pad)
layout(std140, set = 1, binding = 0) uniform uSceneData {
    SceneData scene;
};

// Data specific to this draw call
layout(std140, set = 1, binding = 1) uniform uDrawData {
    mat4 mat_m;
};

void main()
{
    outUv = inUv;
    outNormal = mat3(transpose(inverse(mat_m))) * inNormal;

    vec4 relative_pos = mat_m * vec4(inPos, 1.0);
    if (dimension == 1) {
        outFragPos = relative_pos.xyz;
        gl_Position = mat_viewproj * relative_pos;
        return;
    }

    uint instance = gl_InstanceIndex;
    uint square = dimension * dimension;

    relative_pos.x += float(instance % dimension) * spread;
    relative_pos.y += int(floor(float(instance / dimension))) % dimension * spread;
    relative_pos.z += floor(float(instance / square)) * spread;

    relative_pos.x -= dimension * 2;
    relative_pos.y -= dimension * 2;
    relative_pos.z -= dimension * 2;
    outFragPos = relative_pos.xyz;
    gl_Position = mat_viewproj * relative_pos;
}
