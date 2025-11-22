#pragma once

#include "common/types.h"

#include <SDL3/SDL_gpu.h>
#include <type_traits>

static constexpr size_t PosNormalTangentColorUvAttributeCount = 5;
static constexpr SDL_GPUVertexAttribute
  PosNormalTangentColorUvAttributes[PosNormalTangentColorUvAttributeCount] = {
    { .location = 0,
      .buffer_slot = 0,
      .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
      .offset = 0 },
    { .location = 1,
      .buffer_slot = 0,
      .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
      .offset = FLOAT3 },
    { .location = 2,
      .buffer_slot = 0,
      .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
      .offset = 2 * FLOAT3 },
    { .location = 3,
      .buffer_slot = 0,
      .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
      .offset = 2 * FLOAT3 + FLOAT4 },
    { .location = 4,
      .buffer_slot = 0,
      .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
      .offset = 2 * FLOAT3 + FLOAT4 + FLOAT2 },
  };

static_assert((2 * FLOAT3 + FLOAT4 + FLOAT2 + FLOAT4) ==
              sizeof(PosNormalTangentColorUvVertex));

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

// Casts F to T. Expects T to be an integer type, and F some enum value
template<typename T, typename F>
  requires std::is_integral_v<T> && std::is_enum_v<F>
static constexpr T
CastFlag(F f)
{
  return static_cast<T>(f);
}

// TODO: add more
static std::unordered_map<u32, SDL_GPUTextureFormat> VkToSDL_TextureFormat = {
  { 109, SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT },
  { 97, SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT },
};

#define DISABLE_COPY_AND_MOVE(ClassName)                                       \
  ClassName(const ClassName&) = delete;                                        \
  ClassName& operator=(const ClassName&) = delete;                             \
  ClassName(ClassName&&) = delete;                                             \
  ClassName& operator=(ClassName&&) = delete;

#define DISABLE_COPY(ClassName)                                                \
  ClassName(const ClassName&) = delete;                                        \
  ClassName& operator=(const ClassName&) = delete;

void
disable_blending(SDL_GPUColorTargetDescription& d);

void
enable_blending(SDL_GPUColorTargetDescription& d);
