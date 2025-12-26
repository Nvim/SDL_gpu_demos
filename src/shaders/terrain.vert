#version 450 core

#extension GL_GOOGLE_include_directive : require
#include "terrain_chunk_instance.glsl"
#include "camera_binding.glsl"

struct Vertex {
    vec3 position;
    float pad_0;
};

layout(location = 0) out vec3 OutFragColor;

layout(std430, set = 0, binding = 0) readonly buffer InstanceBuffer {
    ChunkInstance Instances[];
};

layout(std140, set = 1, binding = 0) uniform uCamera {
    CameraBinding camera;
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

const Vertex bot_left = Vertex(vec3(-1.0, 0.0, -1.0), 0.0);
const Vertex bot_right = Vertex(vec3(1.0, 0.0, -1.0), 0.0);
const Vertex top_left = Vertex(vec3(-1.0, 0.0, 1.0), 0.0);
const Vertex top_right = Vertex(vec3(1.0, 0.0, 1.0), 0.0);

const Vertex Quad[4] = Vertex[4](
        bot_left, bot_right, top_left, top_right
    );

void main()
{
    Vertex vert = Quad[gl_VertexIndex];
    ChunkInstance instance = Instances[gl_InstanceIndex];

    vec3 translation = vec3(instance.world_translation.x,
            instance.y_corners[gl_VertexIndex],
            instance.world_translation.y);
    mat4 mat_translate = modelFromWorldPos(translation);
    mat4 mat_scale = scaleWorldPos(translation, 4.f);
    mat4 mat_m = mat_translate * mat_scale;
    vec4 world_pos = mat_m * vec4(vert.position, 1.0);

    OutFragColor = vec3(instance.pad_[0], .55f, instance.pad_[1]);
    gl_Position = camera.viewproj * world_pos;
}
