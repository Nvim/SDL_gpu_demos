#ifndef CHUNK_INSTANCE_GLSL
#define CHUNK_INSTANCE_GLSL

struct ChunkInstance {
    // XZ translation of the quad in range [-C/2, C/2] with C= chunk count
    ivec2 world_translation; 
};

#endif // !CHUNK_INSTANCE_GLSL
