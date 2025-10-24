#pragma once

#include "common/types.h"

#include <SDL3/SDL_gpu.h>
#include <type_traits>

struct PosVertex
{
  float pos[3];
};

struct PosColVertex
{
  float poscol[6];
};

struct PosUvVertex
{
  float pos[3];
  float uv[2];
};

struct PosNormalUvVertex
{
  glm::vec3 pos;
  glm::vec3 normal{ 0.f };
  glm::vec2 uv{ 0.f };
};

struct PosNormalColorUvVertex
{
  glm::vec3 pos;
  glm::vec3 normal{ 0.f };
  glm::vec2 uv{ 0.f };
  glm::vec4 color{ 1.f };
};

struct PosNormalTangentColorUvVertex
{
  glm::vec3 pos;
  glm::vec3 normal{ 0.f };
  glm::vec4 tangent{ 0.f };
  glm::vec2 uv{ 0.f };
  glm::vec4 color{ 1.f };
};

#define RELEASE_IF(ptr, release_func)                                          \
  if (ptr != nullptr) {                                                        \
    release_func(Device, ptr);                                                 \
  }

#define GETERR SDL_GetError()

SDL_GPUShader*
LoadShader(const char* path,
           SDL_GPUDevice* device,
           Uint32 samplerCount,
           Uint32 uniformBufferCount,
           Uint32 storageBufferCount,
           Uint32 storageTextureCount);

SDL_Surface*
LoadImage(const char* path);

template<typename T, typename... U>
SharedPtr<T>
MakeShared(U... args)
{
  return std::make_shared<T>(args...);
}

template<typename T, typename... U>
UniquePtr<T>
MakeUnique(U... args)
{
  return std::make_unique<T>(args...);
}

// Casts F to T. Expects T to be an integer type, and F some enum value
template<typename T, typename F>
  requires std::is_integral_v<T> && std::is_enum_v<F>
static constexpr T
CastFlag(F f)
{
  return static_cast<T>(f);
}
