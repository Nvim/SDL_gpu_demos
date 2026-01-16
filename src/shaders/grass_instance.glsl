#ifndef GRASS_INSTANCE_GLSL
#define GRASS_INSTANCE_GLSL

struct GrassInstance {
    uint chunk_index; // Chunk the grassblade is standing on
    float rotation; // Y axis rotation
    vec2 relative_translation; // Position relative to chunk's center
};

#endif // !GRASS_INSTANCE_GLSL
