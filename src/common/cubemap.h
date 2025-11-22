#pragma once

#include "common/types.h"
#include <SDL3/SDL_gpu.h>
#include <filesystem>

enum class CubeMapUsage : u8
{
  Skybox,
  IrradianceMap,
  SpecularMap,
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
  friend class ProjectionCubemapLoader;

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
  const char* paths_[6]{ "left.jpg",   "right.jpg", "top.jpg",
                         "bottom.jpg", "back.jpg",  "front.jpg" };
};

// Ktx format allows retrieving each face individually
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

// HDR flat equirectangular projection. Reconstructs faces
class ProjectionCubemapLoader final : public ICubemapLoader
{
public:
  explicit ProjectionCubemapLoader(SDL_GPUDevice* device);
  UniquePtr<Cubemap> Load(std::filesystem::path path,
                          CubeMapUsage usage) const override;
  ~ProjectionCubemapLoader() override = default;

private:
  bool CreatePipeline();
  bool UploadVertexData();
  bool CreateSampler();

private:
  bool init_{ false };
  static inline SDL_GPUBuffer* VertexBuffer{ nullptr };
  static inline SDL_GPUBuffer* IndexBuffer{ nullptr };
  static inline SDL_GPUGraphicsPipeline* Pipeline{ nullptr };
  static constexpr const char* VertPath =
    "resources/shaders/compiled/cubemap_projection.vert.spv";
  static constexpr const char* FragPath =
    "resources/shaders/compiled/cubemap_projection.frag.spv";
  SDL_GPUDevice* device_{};
};
