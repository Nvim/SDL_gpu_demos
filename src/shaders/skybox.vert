#version 450 core

#extension GL_GOOGLE_include_directive : require
#include "scene_data.glsl"

layout(location = 0) in vec3 inPos;
layout(location = 0) out vec3 inUv;

layout(std140, set = 1, binding = 0) uniform uSceneData {
    SceneData scene;
};

void main()
{
    inUv = inPos;
    vec4 res = mat_viewproj * mat_cam * vec4(inPos, 1.0).xyzw;
    gl_Position = res.xyww;
}
