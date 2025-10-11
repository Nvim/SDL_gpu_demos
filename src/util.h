#pragma once

#include "src/types.h"
#include <SDL3/SDL_gpu.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
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
  float pos[3];
  float normal[3]{ 0.f };
  float uv[2]{ 0.f };
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
