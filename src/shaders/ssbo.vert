#version 450

struct Vertex {
    vec3 position;
    float _pad0; 
    vec3 color;
    float _pad1;
};

layout(location = 0) out vec3 outColor;
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;
layout(std430, set = 0, binding = 0) buffer VertexBuffer {
    Vertex Vertices[];
};


void main() {
    Vertex vert = Vertices[gl_VertexIndex];
    vec3 position = vert.position;

    outColor = vert.color;
    gl_Position = vec4(position, 1.0);
}
