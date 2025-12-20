#pragma once

#include "common/types.h"

#include <SDL3/SDL_gpu.h>

class Engine;

class Grid
{
public:
  Grid(Engine* engine, SDL_GPUTextureFormat framebuffer_format);
  ~Grid();

  void Draw(SDL_GPUCommandBuffer* cmd_buf,
            SDL_GPURenderPass* pass,
            const CameraBinding& camera_uniform) const;
  bool IsLoaded() const { return loaded_; }

public:
  static constexpr const char* FRAG_PATH =
    "resources/shaders/compiled/grid.frag.spv";
  static constexpr const char* VERT_PATH =
    "resources/shaders/compiled/grid.vert.spv";
  SDL_GPUGraphicsPipeline* Pipeline{ nullptr };

private:
  bool Init();

  Engine* engine_;
  bool loaded_{ false };
  SDL_GPUTextureFormat framebuffer_format_;
};
