#version 450

layout(location = 0) out vec2 uv;
layout(location = 1) out vec2 out_camPos;

#extension GL_GOOGLE_include_directive : require
#include "camera_binding.glsl"

layout(set = 1, binding = 0) uniform uCamera {
    CameraBinding camera;
};

// extents of grid in world coordinates
const float gridSize = 100.0;

const vec4 origin = vec4(0.f, 0.f, 0.f, 0.f);

const vec3 pos[4] = vec3[4](
        vec3(-1.0, 0.0, -1.0),
        vec3(1.0, 0.0, -1.0),
        vec3(1.0, 0.0, 1.0),
        vec3(-1.0, 0.0, 1.0)
    );

const int indices[6] = int[6](
        0, 1, 2, 2, 3, 0
    );

void main() {
    int idx = indices[gl_VertexIndex];
    vec3 position = pos[idx] * gridSize;

    position.x += camera.world_pos.x;
    position.z += camera.world_pos.z;

    position += origin.xyz;

    out_camPos = camera.world_pos.xz;

    gl_Position = camera.viewproj * vec4(position, 1.0);
    uv = position.xz;
}
