#version 450 core

#extension GL_GOOGLE_include_directive : require
#include "camera_binding.glsl"

layout(location = 0) in vec3 inPos;
layout(location = 0) out vec3 outUv;

layout(std140, set = 1, binding = 0) uniform uCameraData {
    CameraBinding camera;
};

void main()
{
    outUv = inPos;
    vec4 res = camera.viewproj * camera.model * vec4(inPos, 1.0).xyzw;
    gl_Position = res.xyww;
}
