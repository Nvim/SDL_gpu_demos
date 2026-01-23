#ifndef CULL_CHUNKS_SETTINGS_H
#define CULL_CHUNKS_SETTINGS_H

#ifdef __cplusplus
using vec3 = glm::vec3;
using uint = std::uint32_t;
#endif

struct CullChunkSettings
{
  uint terrain_width;   // chunk count (single axis)
  uint grass_per_chunk; // grassblades per chunk (single axis)
  uint world_size;
};

#endif // !CULL_CHUNKS_SETTINGS_H
