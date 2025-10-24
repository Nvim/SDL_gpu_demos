#pragma once

#include <array>

#include "common/types.h"
#include "common/ubo.h"
#include "common/util.h"

#include <SDL3/SDL_gpu.h>
#include <glm/vec4.hpp>

enum class PbrTextureFlag : u8
{
  BaseColor = 0,
  MetalRough = 1,
  Normal = 2,
  Occlusion = 3,
  Emissive = 4,
  COUNT
};

static constexpr u32 MaterialBindingSlot = 1;

// Generic material type, holds everything necessary to draw:
// - Pipeline
// - TextureSamplerBindings
// - UBOs
struct MaterialInstance
{
  static constexpr u8 TextureCount =
    CastFlag<u8, PbrTextureFlag>(PbrTextureFlag::COUNT);

  SDL_GPUGraphicsPipeline* Pipeline;
  std::array<SDL_GPUTextureSamplerBinding, TextureCount> SamplerBindings{};
  UBO<MaterialDataBinding> ubo;
  void BindSamplers(SDL_GPURenderPass* pass)
  {
    SDL_BindGPUFragmentSamplers(pass, 0, SamplerBindings.begin(), TextureCount);
  }
};

struct IMaterialBuilder
{
  virtual SharedPtr<MaterialInstance> Build() = 0;
};
