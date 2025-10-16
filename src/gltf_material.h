#pragma once

#include "material.h"
#include "shaders/material_features.h"

#define CAST_FLAG(f) static_cast<u8>(f)

// Maps to the 'pbrMetallicRoughness' attr of GLTF material
struct GLTFPbrMaterial final : public IMaterialBuilder
{
  static constexpr u8 TextureCount = CAST_FLAG(PbrTextureFlag::COUNT);
  // Color factors:
  glm::vec4 BaseColorFactor{ 1.f };
  glm::vec4 EmissiveFactor{ 1.f };
  f32 MetallicFactor{ 1.f };
  f32 RoughnessFactor{ 1.f };
  u32 FeatureFlags{ 0x00 }; // bitfield

  // GPU resources:
  SDL_GPUTexture* BaseColorTexture{ nullptr };
  SDL_GPUSampler* BaseColorSampler{ nullptr };

  SDL_GPUTexture* MetalRoughTexture{ nullptr };
  SDL_GPUSampler* MetalRoughSampler{ nullptr };

  SDL_GPUTexture* NormalTexture{ nullptr };
  SDL_GPUSampler* NormalSampler{ nullptr };

  virtual SharedPtr<MaterialInstance> Build() override;
};
