#version 450 core

#extension GL_GOOGLE_include_directive : require
#include "terrain_chunk_instance.glsl"
#include "camera_binding.glsl"

struct Vertex {
    vec3 position;
    float pad_0;
};

layout(location = 0) out vec3 OutFragColor;

layout(set = 0, binding = 0) uniform sampler2D TexNoise;

layout(std430, set = 0, binding = 1) readonly buffer InstanceBuffer {
    ChunkInstance Instances[];
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

// uniform scale diagonal matrix
mat4 scaleWorldPos(in vec3 worldPos, in float scale) {
    mat4 m = mat4(1.0);
    for (uint i = 0; i < 3; i++) {
        m[i][i] = scale;
    }
    return m;
}

const Vertex bot_left = Vertex(vec3(-.5f, 0.f, -.5f), 0.f);
const Vertex bot_right = Vertex(vec3(.5f, 0.f, -.5f), 0.f);
const Vertex top_left = Vertex(vec3(-.5f, 0.f, .5f), 0.f);
const Vertex top_right = Vertex(vec3(.5f, 0.f, .5f), 0.f);

const Vertex Quad[4] = Vertex[4](
        bot_left, bot_right, top_left, top_right
    );

const vec3 base_color = vec3(.19f, .44f, .12f);

void main()
{
    Vertex vert = Quad[gl_VertexIndex];
    ChunkInstance instance = Instances[gl_InstanceIndex];

    // [0, WIDTH-1]
    // int w = gl_InstanceIndex % terrain_width;
    // int h = gl_InstanceIndex / terrain_width;
    float w = instance.world_translation.x;
    float h = instance.world_translation.y;

    // float world_scale = world_size / terrain_width;
    float world_scale = float(world_size) / float(terrain_width);

    vec3 translation = vec3(w * world_scale, 0.f, h * world_scale);

    mat4 mat_translate = modelFromWorldPos(translation);
    mat4 mat_scale = scaleWorldPos(translation, world_scale);
    mat4 mat_model = mat_translate * mat_scale;
    vec4 world_pos = mat_model * vec4(vert.position, 1.0);

    vec2 uv = vec2(
            ((world_pos.x / world_scale) / terrain_width) + .5f,
            ((world_pos.z / world_scale) / terrain_width) + .5f
        );

    float height = texture(TexNoise, uv).r;
    world_pos.y = (height - .5f) * heightmap_scale;

    gl_Position = camera.viewproj * world_pos;
    if (highlight_chunks != 0) {
        OutFragColor = vec3(vert.position.x+.5f, .2f, vert.position.z+.5f);
        return;
    }
    OutFragColor = base_color * (height * .75f + .25f);
}
