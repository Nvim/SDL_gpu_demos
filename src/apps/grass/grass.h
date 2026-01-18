#pragma once

#include "common/grid.h"
#include "common/program.h"
#include "common/skybox.h"
#include "common/types.h"
#include "shaders/fog_settings.h"
#include "shaders/grass_gen.h"

#include <SDL3/SDL_gpu.h>
#include <imgui/imgui.h>

class GLTFScene;
class Engine;

namespace grass {

struct TerrainBinding
{
  i32 terrain_width;
  i32 world_size;
  f32 heightmap_scale;
  i32 highlight_chunks;
  glm::vec4 terrain_color;
};

class GrassProgram : public Program
{
  using path = std::filesystem::path;

  const path SKYBOX_PATH{ "resources/textures/puresky.hdr" };
  const path GRASS_PATH{ "resources/models/single_grass_blade.glb" };
  static constexpr const char* GRASS_VS_PATH =
    "resources/shaders/compiled/grass.vert.spv";
  static constexpr const char* GRASS_FS_PATH =
    "resources/shaders/compiled/grass.frag.spv";
  static constexpr const char* TERRAIN_VS_PATH =
    "resources/shaders/compiled/terrain.vert.spv";
  static constexpr const char* TERRAIN_FS_PATH =
    "resources/shaders/compiled/terrain.frag.spv";
  static constexpr const char* COMP_PATH =
    "resources/shaders/compiled/generate_grass.comp.spv";
  static constexpr const char* CULL_COMP_PATH =
    "resources/shaders/compiled/cull_chunks.comp.spv";
  static constexpr SDL_GPUTextureFormat TARGET_FORMAT =
    SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  static constexpr SDL_GPUTextureFormat DEPTH_FORMAT =
    SDL_GPU_TEXTUREFORMAT_D16_UNORM;

  static constexpr u64 CHUNK_INSTANCE_SZ = 8;
  static constexpr u64 GRASS_INSTANCE_SZ = 16;

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
  bool CreateGraphicsPipelines();
  bool CreateComputePipelines();
  bool UploadVertexData();
  bool GenerateGrassblades();
  bool CullChunks(CameraBinding& camera);
  void DrawGrass(SDL_GPURenderPass* pass,
                 SDL_GPUCommandBuffer* cmdbuf,
                 CameraBinding& camera);
  void DrawTerrain(SDL_GPURenderPass* pass,
                   SDL_GPUCommandBuffer* cmdbuf,
                   CameraBinding& camera);
  ImDrawData* DrawGui();

private:
  bool quit_{ false };
  bool draw_terrain_{ true };
  bool draw_grass_{ false };
  bool freeze_cull_camera{ false };
  i32 window_w_;
  i32 window_h_;
  i32 rendertarget_w_;
  i32 rendertarget_h_;
  u32 grassblade_index_count_{ 0 };
  Camera camera_{};
  Skybox skybox_{ SKYBOX_PATH, EnginePtr, TARGET_FORMAT };
  DirLightBinding sunlight_;
  TerrainBinding terrain_params_{
    .terrain_width = 16,
    .world_size = 32,
    .heightmap_scale = 4.f,
    .highlight_chunks = 0,
    .terrain_color = { .04f, .12f, .01f, 0.f },
  };
  GrassGenerationParams grass_gen_params_{ .base_color =
                                             glm::vec3{ .19f, .44f, .12f },
                                           .flags =
                                             GRASS_ROTATE | GRASS_OFFSET_POS,
                                           .terrain_width = 16,
                                           .grass_per_chunk = 16,
                                           .offset_cap = .2f,
                                           .density = 12.f };
  FogSettings fog_settings{
    .fog_color = glm::vec3{ 0.714, 0.82, 0.871 },
    .fog_type = FOG_EXP_SQ,
    .fog_density = .75f,
    .fog_end = 100.f,
    .fog_start = 18.f,
  };
  bool regenerate_grass_{ false };

  // GPU Resources:
  SDL_GPUTexture* depth_target_{ nullptr };
  SDL_GPUTexture* scene_target_{ nullptr };
  SDL_GPUTexture* noise_texture_{ nullptr };
  SDL_GPUSampler* noise_sampler_{ nullptr };
  SDL_GPUGraphicsPipeline* grass_pipeline_{ nullptr };
  SDL_GPUGraphicsPipeline* terrain_pipeline_{ nullptr };
  SDL_GPUComputePipeline* generate_grass_pipeline_{ nullptr };
  SDL_GPUComputePipeline* cull_chunks_pipeline_{ nullptr };
  SDL_GPUColorTargetInfo scene_color_target_info_{};
  SDL_GPUDepthStencilTargetInfo scene_depth_target_info_{};
  SDL_GPUColorTargetInfo swapchain_target_info_{};
  SDL_GPUBuffer* grassblade_vertices_{ nullptr };
  SDL_GPUBuffer* grassblade_indices_{ nullptr };
  SDL_GPUBuffer* grassblade_instances_{ nullptr };
  SDL_GPUBuffer* chunk_indices_{ nullptr };
  SDL_GPUBuffer* chunk_instances_{ nullptr };
  SDL_GPUBuffer* visible_chunks_{ nullptr };

  SDL_GPUBuffer* draw_calls_{ nullptr };
};

} // namespace grass
