#pragma once

#include "src/types.h"
#include <SDL3/SDL_gpu.h>
#include <array>
#include <glm/vec4.hpp>

enum class PbrTextureFlag : u8
{
  BaseColor = 0,
  MetalRough = 1,
  Normal = 2,
  COUNT
};
#define CAST_FLAG(f) static_cast<u8>(f)

// maps to the 'pbrMetallicRoughness' attr of GLTF material
struct PbrMaterial
{
  static constexpr u8 TextureCount = CAST_FLAG(PbrTextureFlag::COUNT);
  // factors:
  glm::vec4 BaseColorFactor{ 1.f };
  glm::vec4 EmissiveFactor{ 1.f };
  f32 MetallicFactor{ 1.f };
  f32 RoughnessFactor{ 1.f };

  // GPU resources:
  // SDL_GPUTexture* BaseColorTexture{ nullptr };
  // SDL_GPUSampler* BaseColorSampler{ nullptr };
  //
  // SDL_GPUTexture* MetalRoughTexture{ nullptr };
  // SDL_GPUSampler* MetalRoughSampler{ nullptr };
  //
  // SDL_GPUTexture* NormalTexture{ nullptr };
  // SDL_GPUSampler* NormalSampler{ nullptr };

  std::array<SDL_GPUTextureSamplerBinding, CAST_FLAG(PbrTextureFlag::COUNT)>
    SamplerBindings{};

  void BindSamplers(SDL_GPURenderPass* pass)
  {
    SDL_BindGPUFragmentSamplers(pass, 0, SamplerBindings.begin(), TextureCount);
  }

  // SDL_GPUGraphicsPipeline* pipeline{ nullptr };
};
