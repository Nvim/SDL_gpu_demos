#pragma once

#include "common/program.h"
#include "common/skybox.h"
#include "common/types.h"

#include <SDL3/SDL_gpu.h>
#include <imgui/imgui.h>

class GLTFScene;
class Engine;

namespace grass {

class GrassProgram : public Program
{
  using path = std::filesystem::path;

  const path SKYBOX_PATH{ "resources/textures/puresky.hdr" };
  const path GRASS_PATH{ "resources/models/single_grass_blade.glb" };
  static constexpr const char* VS_PATH =
    "resources/shaders/compiled/grass.vert.spv";
  static constexpr const char* FS_PATH =
    "resources/shaders/compiled/grass.frag.spv";
  static constexpr const char* COMP_PATH =
    "resources/shaders/compiled/generate_grass.comp.spv";
  static constexpr SDL_GPUTextureFormat TARGET_FORMAT =
    SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  static constexpr SDL_GPUTextureFormat DEPTH_FORMAT =
    SDL_GPU_TEXTUREFORMAT_D16_UNORM;

  static constexpr u32 GRID_SZ = 16;

public:
  GrassProgram(SDL_GPUDevice* device,
               SDL_Window* window,
               Engine* engine,
               i32 w,
               i32 h);
  bool Init() override;
  bool Poll() override;
  bool Draw() override;
  bool ShouldQuit() override;
  ~GrassProgram();

private:
  bool InitGui();
  bool CreateRenderTargets();
  bool CreatePipeline();
  bool CreateComputePipeline();
  bool UploadVertexData();
  ImDrawData* DrawGui();

private:
  bool quit_{ false };
  i32 window_w_;
  i32 window_h_;
  i32 rendertarget_w_;
  i32 rendertarget_h_;
  u32 grid_size_{ 32 };
  u32 index_count_{ 0 };
  Camera camera_{};
  Skybox skybox_{ SKYBOX_PATH, EnginePtr, TARGET_FORMAT };

  // GPU Resources:
  SDL_GPUTexture* depth_target_{ nullptr };
  SDL_GPUTexture* scene_target_{ nullptr };
  SDL_GPUGraphicsPipeline* pipeline_{ nullptr };
  SDL_GPUComputePipeline* generate_grass_pipeline_{ nullptr };
  SDL_GPUColorTargetInfo scene_color_target_info_{};
  SDL_GPUDepthStencilTargetInfo scene_depth_target_info_{};
  SDL_GPUColorTargetInfo swapchain_target_info_{};
  SDL_GPUBuffer* index_buffer_{ nullptr };
  SDL_GPUBuffer* instance_buffer_{ nullptr };
  SDL_GPUBuffer* vertex_ssbo_{ nullptr };
};

} // namespace grass
