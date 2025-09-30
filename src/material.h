#pragma once

#include "src/types.h"
#include <SDL3/SDL_gpu.h>
#include <glm/vec4.hpp>

// maps to the 'pbrMetallicRoughness' attr of GLTF material
struct PbrMaterial
{
  // factors:
  glm::vec4 BaseColorFactor{1.f};
  glm::vec4 EmissiveFactor{1.f};
  f32 MetallicFactor{1.f};
  f32 RoughnessFactor{1.f};

  // GPU resources:
  SDL_GPUTexture* BaseColorTexture{ nullptr };
  SDL_GPUSampler* BaseColorSampler{ nullptr };

  SDL_GPUTexture* MetalRoughTexture{ nullptr };
  SDL_GPUSampler* MetalRoughSampler{ nullptr };

  // SDL_GPUGraphicsPipeline* pipeline{ nullptr };
};
