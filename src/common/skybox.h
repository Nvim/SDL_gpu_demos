#pragma once

#include "common/rendersystem.h"
#include "common/types.h"

#include <SDL3/SDL_gpu.h>

class Engine;
struct Cubemap;

class Skybox
{
public:
  explicit Skybox(const std::filesystem::path path,
                  Engine* engine,
                  SDL_GPUTextureFormat framebuffer_format);
  explicit Skybox(const std::filesystem::path path,
                  const char* vert_path,
                  const char* frag_path,
                  Engine* engine,
                  SDL_GPUTextureFormat framebuffer_format);
  ~Skybox();

  bool IsLoaded() const { return loaded_; }
  void Draw(SDL_GPUCommandBuffer* cmd_buf,
            SDL_GPURenderPass* pass,
            const CameraBinding& camera_uniform) const;

public:
  static constexpr const char* FRAG_PATH =
    "resources/shaders/compiled/skybox.frag.spv";
  const char* VertPath = "resources/shaders/compiled/skybox.vert.spv";
  const char* FragPath = FRAG_PATH;
  UniquePtr<Cubemap> Cubemap{};
  SDL_GPUSampler* CubemapSampler{};
  MeshBuffers Buffers{};
  SDL_GPUGraphicsPipeline* Pipeline{ nullptr };

private:
  bool Init();
  bool CreatePipeline();

private:
  const std::filesystem::path path_{};
  Engine* engine_;
  bool loaded_{ false };
  SDL_GPUTextureFormat framebuffer_format_;
};
