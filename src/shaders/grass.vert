#version 450 core

#extension GL_GOOGLE_include_directive : require
#include "grass_instance.glsl"

struct Vertex {
    vec3 position;
    float pad_0;
    vec3 normal;
    float pad_1;
};

layout(location = 0) out vec3 OutFragColor;
layout(location = 1) out vec3 OutViewPos;
layout(location = 2) out vec3 OutFragPos;
layout(location = 3) out vec3 OutNormal;

layout(std430, set = 0, binding = 0) buffer VertexBuffer {
    Vertex Vertices[];
};

layout(std430, set = 0, binding = 1) readonly buffer InstanceBuffer {
    GrassInstance Instances[];
};

// Data global to whole scene (120 bytes data, 8 to pad)
layout(std140, set = 1, binding = 0) uniform uCamera {
    mat4 mat_viewproj;
    mat4 mat_cam;
    vec4 camera_world;
};

mat4 modelFromWorldPos(in vec3 worldPos) {
    mat4 m = mat4(1.0);
    m[3] = vec4(worldPos, 1.0);
    return m;
}

mat4 rotateY(float angle)
{
    float c = cos(angle);
    float s = sin(angle);

    // |  c   0   s   0 |
    // |  0   1   0   0 |
    // | -s   0   c   0 |
    // |  0   0   0   1 |
    return mat4(
        vec4(c, 0.f, -s, 0.f), // column 0
        vec4(0.f, 1.f, 0.f, 0.f), // column 1
        vec4(s, 0.f, c, 0.f), // column 2
        vec4(0.f, 0.f, 0.f, 1.f) // column 3
    );
}

void main()
{
    Vertex vert = Vertices[gl_VertexIndex];
    GrassInstance instance = Instances[gl_InstanceIndex];

    vec3 inPos = vert.position;
    mat4 mat_m = modelFromWorldPos(instance.world_pos);
    mat_m *= rotateY(instance.rotation);
    vec4 relative_pos = mat_m * vec4(inPos, 1.0);

    OutFragColor = instance.color;
    OutFragPos = relative_pos.xyz;
    OutViewPos = camera_world.xyz;
    OutNormal = mat3(transpose(inverse(mat_m))) * vert.normal;
    gl_Position = mat_viewproj * relative_pos;
}
