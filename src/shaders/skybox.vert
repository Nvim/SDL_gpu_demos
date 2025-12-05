#version 450 core

layout(location = 0) in vec3 inPos;
layout(location = 0) out vec3 outUv;

layout(std140, set = 1, binding = 0) uniform uCameraData {
    mat4 mat_viewproj;
    mat4 mat_cam;
    vec4 camera_world;
};

void main()
{
    outUv = inPos;
    vec4 res = mat_viewproj * mat_cam * vec4(inPos, 1.0).xyzw;
    gl_Position = res.xyww;
}
