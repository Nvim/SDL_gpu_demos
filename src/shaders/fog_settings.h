#ifndef FOG_SETTINGS_H
#define FOG_SETTINGS_H

#ifdef __cplusplus
using vec4 = glm::vec4;
using vec3 = glm::vec3;
using uint = std::uint32_t;
#endif

// clang-format off
#define FOG_NONE   (0x01 << 0x00)
#define FOG_LINEAR (0x01 << 0x01)
#define FOG_EXP    (0x01 << 0x02)
#define FOG_EXP_SQ (0x01 << 0x03)
// clang-format on

struct FogSettings
{
  vec3 fog_color;
  uint fog_type;
  float fog_density;
  float fog_end;
  float fog_start;
};

#endif // !FOG_SETTINGS_H
