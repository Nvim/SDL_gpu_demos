#pragma once

#include "common/cubemap.h"
#include "common/types.h"
#include "common/util.h"

#include <SDL3/SDL_gpu.h>

class Skybox
{
public:
  explicit Skybox(const std::filesystem::path path,
                  SDL_Window* window,
                  SDL_GPUDevice* device);
  explicit Skybox(const std::filesystem::path path,
                  const char* vert_path,
                  const char* frag_path,
                  SDL_Window* window,
                  SDL_GPUDevice* device);
  ~Skybox();

  bool IsLoaded() const { return loaded_; }
  void Draw(SDL_GPURenderPass* pass) const;

public:
  static constexpr const char* FRAG_PATH =
    "resources/shaders/compiled/skybox.frag.spv";
  const char* VertPath = "resources/shaders/compiled/skybox.vert.spv";
  const char* FragPath = FRAG_PATH;
  UniquePtr<Cubemap> Cubemap{};
  SDL_GPUSampler* CubemapSampler{};
  SDL_GPUBuffer* VertexBuffer{};
  SDL_GPUBuffer* IndexBuffer{};
  SDL_GPUGraphicsPipeline* Pipeline{ nullptr };

private:
  bool Init();
  bool CreatePipeline();
  bool SendVertexData() const;

private:
  const std::filesystem::path path_{};
  SDL_GPUDevice* device_{}; // needed for dtor
  SDL_Window* window_{};    // needed swapchain format
  bool loaded_{ false };
};
