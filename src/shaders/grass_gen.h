#ifndef GRASS_GEN_H

// clang-format off
#define GRASS_ROTATE       (0x01 << 0x00)
#define GRASS_OFFSET_POS   (0x01 << 0x01)

#ifdef __cplusplus
using vec3 = glm::vec3;
using uint = std::uint32_t;
#endif

struct GrassGenerationParams {
    vec3 base_color;
    uint flags;
    uint terrain_width; // dispatch count of 1 dimension
    uint grass_per_chunk; // local_size of 1 dimension
    float offset_cap; // world pos per-blade offset cap

    // TODO: control chunk size in world space and compute density instead
    float density;
};

#endif // !GRASS_GEN_H
