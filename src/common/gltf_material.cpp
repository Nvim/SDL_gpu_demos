#include <pch.h>

#include "gltf_material.h"

#include "common/material.h"
#include "common/ubo.h"

#define CREATE_BINDING(flag, tex, sampler)                                     \
  mat->SamplerBindings[CastFlag<u8>(PbrTextureFlag::flag)] =                   \
    SDL_GPUTextureSamplerBinding{ (tex), (sampler) };

SharedPtr<MaterialInstance>
GLTFPbrMaterial::Build()
{
  auto mat = std::make_shared<MaterialInstance>();

  mat->Pipeline = nullptr; // TODO: figure out pipeline ownership

  CREATE_BINDING(BaseColor, BaseColorTexture, BaseColorSampler);
  CREATE_BINDING(MetalRough, MetalRoughTexture, MetalRoughSampler);
  CREATE_BINDING(Normal, NormalTexture, NormalSampler);
  CREATE_BINDING(Occlusion, OcclusionTexture, OcclusionSampler);
  CREATE_BINDING(Emissive, EmissiveTexture, EmissiveSampler);

  MaterialDataBinding binding{};
  {
    binding.color_factors = BaseColorFactor;
    binding.metal_factor = MetallicFactor;
    binding.rough_factor = RoughnessFactor;
    binding.feature_flags = FeatureFlags;
  };
  mat->ubo =
    UBO(UBOBindingType::Fragment, MaterialBindingSlot, std::move(binding));
  return mat;
}
#undef CREATE_BINDING
