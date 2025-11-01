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

  friend class MultifileCubemapLoader;
  friend class KtxCubemapLoader;

private: // TODO: remove this in favor of RAII texture wrapper
  SDL_GPUDevice* device_{};
};

class ICubemapLoader
{
public:
  virtual UniquePtr<Cubemap> Load(std::filesystem::path dir,
                                  CubeMapUsage usage) const = 0;
  virtual ~ICubemapLoader() = default;
};

class MultifileCubemapLoader final : public ICubemapLoader
{
public:
  explicit MultifileCubemapLoader(SDL_GPUDevice* device);
  UniquePtr<Cubemap> Load(std::filesystem::path dir,
                          CubeMapUsage usage) const override;
  ~MultifileCubemapLoader() override = default;

private:
  SDL_GPUDevice* device_{};
  // TODO: hdr extension support
  const char* paths_[6]{ "left.jpg",   "right.jpg", "top.jpg",
                         "bottom.jpg", "back.jpg",  "front.jpg" };
};

class KtxCubemapLoader final : public ICubemapLoader
{
public:
  explicit KtxCubemapLoader(SDL_GPUDevice* device);
  UniquePtr<Cubemap> Load(std::filesystem::path path,
                          CubeMapUsage usage) const override;
  ~KtxCubemapLoader() override = default;

private:
  SDL_GPUDevice* device_{};
};
