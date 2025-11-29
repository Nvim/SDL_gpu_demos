#pragma once

#include "common/program.h"
#include "common/types.h"

#include <SDL3/SDL_gpu.h>
#include <imgui/imgui.h>

class GLTFScene;
class Engine;

class GrassProgram : public Program
{
  using path = std::filesystem::path;

  static constexpr const char* VS_PATH =
    "resources/shaders/compiled/grass.vert.spv";
  static constexpr const char* FS_PATH =
    "resources/shaders/compiled/grass.frag.spv";
  static constexpr SDL_GPUTextureFormat TARGET_FORMAT =
    SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

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
  ImDrawData* DrawGui();

private:
  bool quit_{ false };
  i32 window_w_;
  i32 window_h_;
  i32 rendertarget_w_;
  i32 rendertarget_h_;
  // Camera camera_;
  // Skybox skybox_{ PBR_PATH / "skybox.hdr", EnginePtr, HDR_TARGET_FORMAT };

  // GPU Resources:
  SDL_GPUTexture* depth_target_{ nullptr };
  SDL_GPUTexture* scene_target_{ nullptr };
  SDL_GPUColorTargetInfo scene_color_target_info_{};
  SDL_GPUDepthStencilTargetInfo scene_depth_target_info_{};
  SDL_GPUColorTargetInfo swapchain_target_info_{};
};
