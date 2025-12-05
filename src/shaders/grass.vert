#version 450 core

// layout(location = 0) in vec3 inPos;

struct Vertex {
    vec3 position;
    float pad_;
};

// Vertex data SSBO
layout(std430, set = 0, binding = 0) buffer VertexBuffer {
    Vertex Vertices[];
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
    Vertex vert = Vertices[gl_VertexIndex];
    vec3 inPos = vert.position;
    vec4 relative_pos = mat_m * vec4(inPos, 1.0);
    // vec4 relative_pos = vec4(inPos, 1.0);

    // if (dimension == 1) {
    // outFragPos = relative_pos.xyz;
    gl_Position = mat_viewproj * relative_pos;
    return;
    // }

    // uint instance = gl_InstanceIndex;
    // uint square = dimension * dimension;
    //
    // relative_pos.x += float(instance % dimension) * spread;
    // relative_pos.y += int(floor(float(instance / dimension))) % dimension * spread;
    // relative_pos.z += floor(float(instance / square)) * spread;
    //
    // relative_pos.x -= dimension * 2;
    // relative_pos.y -= dimension * 2;
    // relative_pos.z -= dimension * 2;
    // outFragPos = relative_pos.xyz;
    // gl_Position = mat_viewproj * relative_pos;
}
