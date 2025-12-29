#version 450 core

#extension GL_GOOGLE_include_directive : require
#include "grass_instance.glsl"
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

layout(std430, set = 0, binding = 2) readonly buffer InstanceBuffer {
    GrassInstance Instances[];
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
    GrassInstance instance = Instances[gl_InstanceIndex];

    float world_scale = float(world_size) / float(terrain_width);
    vec3 translation = vec3(instance.world_pos.x * world_scale,
            1.f,
            instance.world_pos.z * world_scale);

    vec2 uv = vec2(
            (instance.world_pos.x / terrain_width) + .5f,
            (instance.world_pos.z / terrain_width) + .5f
        );
    float height = texture(TexNoise, uv).r;

    translation.y = (height - .5f) * heightmap_scale;
    mat4 mat_translate = modelFromWorldPos(translation);
    // mat4 mat_scale = scaleWorldPos(translation, world_scale);
    mat4 mat_model = mat_translate;
    mat_model *= rotateY(instance.rotation);
    vec4 world_pos = mat_model * vec4(vert.position, 1.0);

    // vec2 uv = vec2(
    //         ((world_pos.x / world_scale) / terrain_width) + .5f,
    //         ((world_pos.z / world_scale) / terrain_width) + .5f
    //     );
    // float height = texture(TexNoise, uv).r;
    // world_pos.y = (height - .5f) * heightmap_scale;
    // world_pos = modelFromWorldPos(vec3(0.f, (height - .5f) * heightmap_scale, 0.f)) * world_pos;

    OutFragColor = base_color;
    OutFragPos = world_pos.xyz;
    OutViewPos = camera.world_pos.xyz;
    OutNormal = mat3(transpose(inverse(mat_model))) * vert.normal;
    gl_Position = camera.viewproj * world_pos;
}
