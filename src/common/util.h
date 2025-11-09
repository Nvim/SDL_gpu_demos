#pragma once

#include "common/types.h"

#include <SDL3/SDL_gpu.h>
#include <type_traits>

static constexpr size_t FLOAT4 = sizeof(glm::vec4);
static constexpr size_t FLOAT3 = sizeof(glm::vec3);
static constexpr size_t FLOAT2 = sizeof(glm::vec2);
static_assert(FLOAT4 == 16);
static_assert(FLOAT3 == 12);
static_assert(FLOAT2 == 8);

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
