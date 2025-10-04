#version 450 core

layout(location = 0) in vec3 Pos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;

layout(location = 0) out vec2 uv;
layout(location = 1) out vec3 norm;
layout(location = 2) out vec3 FragPos;
layout(location = 3) out vec3 ViewPos;

layout(std140, binding = 0, set = 1) uniform uMatrices {
    mat4 mat_vp;
    mat4 mat_m;
} mvp;

layout(std140, binding = 1, set = 1) uniform uCameraMatrix {
    mat4 mat_cam;
    vec4 camera_world;
};

layout(std140, binding = 2, set = 1) uniform uInstanceSettings {
    float spread;
    uint dimension;
};

void main()
{
    uv = inUv;
    norm = mat3(transpose(inverse(mvp.mat_m))) * inNormal;  
    ViewPos = camera_world.xyz;

    vec4 relative_pos = mvp.mat_m * vec4(Pos, 1.0);
    if (dimension == 1) {
        FragPos = relative_pos.xyz;
        gl_Position = mvp.mat_vp * relative_pos;
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
    FragPos = relative_pos.xyz;
    gl_Position = mvp.mat_vp * relative_pos;
}
