#pragma once

#include "common/types.h"
#include <SDL3/SDL_gpu.h>
#include <filesystem>

enum class CubeMapUsage : u8
{
  Skybox,
  IrradianceMap,
};

struct Cubemap
{
  ~Cubemap();
  CubeMapUsage Usage{ CubeMapUsage::Skybox };
  std::filesystem::path Path{};
  SDL_GPUTextureFormat Format{ SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM };
  SDL_GPUTexture* Texture{}; // actual cube texture

  friend class MultifileCubeMapLoader;

private: // TODO: remove this in favor of RAII texture wrapper
  SDL_GPUDevice* device_{};
};

class MultifileCubeMapLoader
{
public:
  explicit MultifileCubeMapLoader(SDL_GPUDevice* device);
  UniquePtr<Cubemap> Load(
    std::filesystem::path dir,
    CubeMapUsage usage,
    SDL_GPUTextureFormat format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM) const;

private:
  SDL_GPUDevice* device_{};
  // TODO: hdr extension support
  const char* paths_[6]{ "left.jpg",   "right.jpg", "top.jpg",
                         "bottom.jpg", "back.jpg",  "front.jpg" };
};
