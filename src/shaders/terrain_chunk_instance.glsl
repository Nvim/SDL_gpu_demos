#ifndef CHUNK_INSTANCE_GLSL
#define CHUNK_INSTANCE_GLSL

struct ChunkInstance {
    vec4 y_corners; // y level of each vertex of the quad
    vec2 world_translation; // xz translation of the quad
    vec2 pad_;
};

#endif
