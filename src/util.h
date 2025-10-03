#pragma once

#include <SDL3/SDL_gpu.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

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
