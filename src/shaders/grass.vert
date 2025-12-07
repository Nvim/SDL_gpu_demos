#version 450 core

#extension GL_GOOGLE_include_directive : require
#include "grass_instance.glsl"

struct Vertex {
    vec3 position;
    float pad_;
};

layout(location = 0) out vec3 OutFragColor;

layout(std430, set = 0, binding = 0) buffer VertexBuffer {
    Vertex Vertices[];
};

layout(std430, set = 0, binding = 1) readonly buffer InstanceBuffer {
    GrassInstance Instances[];
};

// Data global to whole scene (120 bytes data, 8 to pad)
layout(std140, set = 1, binding = 0) uniform uCamera {
    mat4 mat_viewproj;
    vec4 camera_world;
};

layout(std140, set = 1, binding = 1) uniform uInstanceData {
    mat4 mat_m;
};

void main()
{
    const uint dimension = 16;
    const uint spread = 5;
    Vertex vert = Vertices[gl_VertexIndex];
    GrassInstance instanceData = Instances[gl_InstanceIndex];
    vec3 inPos = vert.position;
    vec4 relative_pos = mat_m * vec4(inPos, 1.0);

    uint instance = gl_InstanceIndex;
    uint square = dimension * dimension;

    relative_pos.x += float(instance % dimension) * spread;
    relative_pos.z += int(floor(float(instance / dimension))) % dimension * spread;
    //
    relative_pos.x -= dimension * 2;
    relative_pos.z -= dimension * 2;

    // outFragPos = relative_pos.xyz;
    OutFragColor = instanceData.color;
    gl_Position = mat_viewproj * relative_pos;
}
