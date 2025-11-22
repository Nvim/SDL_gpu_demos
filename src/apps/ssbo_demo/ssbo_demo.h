#pragma once

#include <SDL3/SDL_gpu.h>
#include <glm/ext/vector_float3.hpp>

#include <common/program.h>

struct PaddedVertex
{
  glm::vec3 pos;
  f32 pad0_;
  glm::vec3 color;
  f32 pad1_;
};

class BufferApp : public Program
{
public:
  BufferApp(SDL_GPUDevice* device,
            SDL_Window* window,
            Engine* engine,
            int w,
            int h);
  bool Init() override;
  bool Poll() override;
  bool Draw() override;
  bool ShouldQuit() override;
  ~BufferApp();

public:
  SDL_GPUGraphicsPipeline* ScenePipeline{ nullptr };

private:
  bool LoadShaders();
  bool CreateSceneRenderTargets();
  bool SendVertexData();

private:
  // Internals:
  bool quit{ false };
  const char* vertex_path_ = "resources/shaders/compiled/ssbo.vert.spv";
  const char* fragment_path_ = "resources/shaders/compiled/color.frag.spv";
  const int vp_width_{ 640 };
  const int vp_height_{ 480 };

  // GPU Resources:
  SDL_GPUBuffer* ssbo_;
  SDL_GPUBuffer* vertex_buffer_;
  SDL_GPUBuffer* index_buffer_;
  SDL_GPUShader* vertex_{ nullptr };
  SDL_GPUShader* fragment_{ nullptr };
  SDL_GPUColorTargetInfo swapchain_target_info_{};

#define RED 1.0, 0.0, 0.0
#define GREEN 0.0, 1.0, 0.0
#define BLUE 0.0, 0.0, 1.0
#define YELLOW 1.0, 1.0, 0.0
#define PURPLE 1.0, 0.0, 1.0
#define CYAN 0.0, 1.0, 1.0

  // clang-format off
  static constexpr Uint8 VERT_COUNT = 4;
  static constexpr PaddedVertex padded_verts[VERT_COUNT] = {
    {glm::vec3{-.6f,  .6f, 0}, 0.f, glm::vec3{PURPLE}, 0.f},
    {glm::vec3{ .6f,  .6f, 0}, 0.f, glm::vec3{CYAN}, 0.f},
    {glm::vec3{ .6f, -.6f, 0}, 0.f, glm::vec3{CYAN}, 0.f},
    {glm::vec3{-.6f, -.6f, 0}, 0.f, glm::vec3{BLUE}, 0.f},
  };
  static constexpr PosColVertex verts[VERT_COUNT] = {
    {-.6f,  .6f, 0, PURPLE},
    { .6f,  .6f, 0, CYAN},
    { .6f, -.6f, 0, CYAN},
    {-.6f, -.6f, 0, BLUE},
  };


  static constexpr Uint8 INDEX_COUNT = 6;
  static constexpr Uint16 indices[INDEX_COUNT] = {
    0, 1, 2,
    0, 2, 3,
  };
  // clang-format on
};
