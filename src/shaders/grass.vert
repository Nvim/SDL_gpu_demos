#version 450 core

#extension GL_GOOGLE_include_directive : require
#include "grass_instance.glsl"
#include "terrain_chunk_instance.glsl"
#include "camera_binding.glsl"

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

layout(set = 0, binding = 0) uniform sampler2D TexNoise;

layout(std430, set = 0, binding = 1) readonly buffer VertexBuffer {
    Vertex Vertices[];
};

layout(std430, set = 0, binding = 2) readonly buffer GrassInstanceBuffer {
    GrassInstance GrassInstances[];
};

layout(std430, set = 0, binding = 3) readonly buffer ChunkInstanceBuffer {
    ChunkInstance ChunkInstances[];
};

layout(std140, set = 1, binding = 0) uniform uCamera {
    CameraBinding camera;
};

layout(std140, set = 1, binding = 1) uniform uTerrain {
    int terrain_width;
    int world_size;
    float heightmap_scale; // different scale for Y
    int highlight_chunks;
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

// uniform scale diagonal matrix
mat4 scaleWorldPos(in vec3 worldPos, in float scale) {
    mat4 m = mat4(1.0);
    for (uint i = 0; i < 3; i++) {
        m[i][i] = scale;
    }
    return m;
}

const vec3 base_color = vec3(.19f, .44f, .12f);
void main()
{
    Vertex vert = Vertices[gl_VertexIndex];
    GrassInstance grass_instance = GrassInstances[gl_InstanceIndex];
    ChunkInstance chunk = ChunkInstances[grass_instance.chunk_index];

    float world_scale = float(world_size) / float(terrain_width);
    vec3 translation = vec3(chunk.world_translation.x * world_scale,
            1.f,
            chunk.world_translation.y * world_scale);

    vec2 uv = vec2(
            ((chunk.world_translation.x) / float(terrain_width)) + .5f,
            ((chunk.world_translation.y) / float(terrain_width)) + .5f
        );
    float height = texture(TexNoise, uv).r;

    translation.y = (height - .5f) * heightmap_scale;
    mat4 mat_translate = modelFromWorldPos(translation);
    mat4 mat_model = mat_translate;
    mat_model *= rotateY(grass_instance.rotation);
    vec4 world_pos = mat_model * vec4(vert.position, 1.0);

    OutFragColor = base_color;
    OutFragPos = world_pos.xyz;
    OutViewPos = camera.world_pos.xyz;
    OutNormal = mat3(transpose(inverse(mat_model))) * vert.normal;
    gl_Position = camera.viewproj * world_pos;
}
