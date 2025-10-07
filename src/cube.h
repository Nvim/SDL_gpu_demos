#pragma once

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_stdinc.h>
#include <imgui/imgui.h>

#include "camera.h"
#include "program.h"
#include "skybox.h"
#include "src/gltf_loader.h"
#include "transform.h"
#include "util.h"

struct Rotation
{
  const char* name;
  float* axis;
  float speed;
};

struct MatricesBinding
{
  glm::mat4 viewProj;
  glm::mat4 objModel;
  // glm::mat4 cameraModel;
};

struct CameraBinding
{
  glm::mat4 cameraMatrix;
  glm::vec3 cameraPos;
};

struct SceneDataBinding
{
  glm::mat4 viewproj;
  glm::mat4 cam_model;
  glm::vec4 camera_world;
  glm::vec4 sun_dir;
  glm::vec4 sun_color;
  f32 spread;
  u32 dimension;
  f32 _pad[2] = { 0.f };
};

struct DrawDataBinding
{
  glm::mat4 model;
  // u32 material_index;
};

struct MaterialDataBinding
{
  glm::vec4 color_factors;
  glm::vec4 metal_rough_factors;
};

struct InstancingCfg
{
  float spread = 5.f;   // gap between each mesh instance
  Uint32 dimension = 1; // instance count per side
};

class CubeProgram : public Program
{
public:
  CubeProgram(SDL_GPUDevice* device,
              SDL_Window* window,
              const char* vertex_path,
              const char* fragment_path,
              int w,
              int h);
  bool Init() override;
  bool Poll() override;
  bool Draw() override;
  bool ShouldQuit() override;
  ~CubeProgram();

private:
  bool InitGui();
  bool LoadShaders();
  bool CreateSceneRenderTargets();
  ImDrawData* DrawGui();
  void UpdateScene();

private:
  // Internals:
  bool quit{ false };
  Transform cube_transform_;
  Camera camera_{ glm::radians(60.0f), 640 / 480.f, .1f, 100.f };
  Skybox skybox_{ "resources/textures/skybox", Window, Device };
  // GLTFLoader loader_{
  //   this,
  //   "resources/models/BarramundiFishGLTF/BarramundiFish.gltf"
  // };
  // GLTFLoader loader_{ this, "resources/models/BarramundiFish.glb" };
  // GLTFLoader loader_{ this, "resources/models/CubeGLTF/Cube.gltf" };
  GLTFLoader loader_{ this,
                      "resources/models/DamagedHelmet/DamagedHelmet.gltf" };
  const char* vertex_path_;
  const char* fragment_path_;
  const int vp_width_{ 640 };
  const int vp_height_{ 480 };

  // User controls:
  Rotation rotations_[3]; // spin cube
  InstancingCfg instance_cfg{};
  bool wireframe_{ false };
  i32 tex_idx{ 0 };

  // GPU Resources:
  SDL_GPUTexture* depth_target_{ nullptr };
  SDL_GPUTexture* color_target_{ nullptr };
  // TODO: store scene-related GPU Resources in GLTF scene class
  SDL_GPUShader* vertex_{ nullptr };
  SDL_GPUShader* fragment_{ nullptr };
  SDL_GPUGraphicsPipeline* scene_pipeline_{ nullptr };
  SDL_GPUGraphicsPipeline* scene_wireframe_pipeline_{ nullptr };
  SDL_GPUColorTargetInfo scene_color_target_info_{};
  SDL_GPUDepthStencilTargetInfo scene_depth_target_info_{};
  SDL_GPUColorTargetInfo swapchain_target_info_{};

#define RED 1.0, 0.0, 0.0
#define GREEN 0.0, 1.0, 0.0
#define BLUE 0.0, 0.0, 1.0
#define YELLOW 1.0, 1.0, 0.0
#define PURPLE 1.0, 0.0, 1.0
#define CYAN 0.0, 1.0, 1.0

  // clang-format off
  static constexpr Uint8 VERT_COUNT = 24;
  static constexpr PosColVertex verts[VERT_COUNT] = {
    // {-.6f,  .6f, 0, PURPLE},
    // { .6f,  .6f, 0, CYAN},
    // { .6f, -.6f, 0, CYAN},
    // {-.6f, -.6f, 0, BLUE},
		{ -0.6f, -0.6f, -0.6f, RED},
		{ 0.6f, -0.6f, -0.6f,  RED},
		{ 0.6f, 0.6f, -0.6f,   RED},
		{ -0.6f, 0.6f, -0.6f,  RED},

		{ -0.6f, -0.6f, 0.6f, YELLOW},
		{ 0.6f, -0.6f, 0.6f,  YELLOW},
		{ 0.6f, 0.6f, 0.6f,   YELLOW},
		{ -0.6f, 0.6f, 0.6f,  YELLOW},

		{ -0.6f, -0.6f, -0.6f, PURPLE},
		{ -0.6f, 0.6f, -0.6f,  PURPLE},
		 { -0.6f, 0.6f, 0.6f,  PURPLE},
		 { -0.6f, -0.6f, 0.6f, PURPLE},

		 { 0.6f, -0.6f, -0.6f, GREEN},
		 { 0.6f, 0.6f, -0.6f,  GREEN},
		 { 0.6f, 0.6f, 0.6f,   GREEN},
		 { 0.6f, -0.6f, 0.6f,  GREEN},

		 { -0.6f, -0.6f, -0.6f, CYAN},
		 { -0.6f, -0.6f, 0.6f,  CYAN},
		 { 0.6f, -0.6f, 0.6f,   CYAN},
		 { 0.6f, -0.6f, -0.6f,  CYAN},

		 { -0.6f, 0.6f, -0.6f, BLUE},
		 { -0.6f, 0.6f, 0.6f,  BLUE},
		 { 0.6f, 0.6f, 0.6f,   BLUE},
		 { 0.6f, 0.6f, -0.6f,  BLUE},
  };

  static constexpr PosUvVertex verts_uvs[VERT_COUNT] = {
		{ {-1.f, -1.f, 1.f},  {0.f, 0.f}},
		{ { 1.f, -1.f, 1.f},  {1.f, 0.f}},
		{ { 1.f,  1.f, 1.f},  {1.f, 1.f}},
		{ {-1.f,  1.f, 1.f},  {0.f, 1.f}},

		{ { 1.f, -1.f, -1.f}, {0.f, 0.f}},
		{ {-1.f, -1.f, -1.f}, {1.f, 0.f}},
		{ {-1.f,  1.f, -1.f}, {1.f, 1.f}},
		{ { 1.f,  1.f, -1.f}, {0.f, 1.f}},

		{ {-1.f, -1.f, -1.f}, {0.f, 0.f}},
		{ {-1.f, -1.f,  1.f}, {1.f, 0.f}},
    { {-1.f,  1.f,  1.f}, {1.f, 1.f}},
		{ {-1.f,  1.f, -1.f}, {0.f, 1.f}},

    { {1.f, -1.f, -1.f},  {0.f, 0.f}},
    { {1.f, -1.f,  1.f},  {1.f, 0.f}},
    { {1.f,  1.f,  1.f},  {1.f, 1.f}},
    { {1.f,  1.f, -1.f},  {0.f, 1.f}},

    { {-1.f, 1.f, -1.f}, {0.f, 0.f}},
    { { 1.f, 1.f, -1.f}, {1.f, 0.f}},
    { { 1.f, 1.f,  1.f}, {1.f, 1.f}},
    { {-1.f, 1.f,  1.f}, {0.f, 1.f}},

    { { 1.f, -1.f, -1.f}, {0.f, 0.f}},
    { {-1.f, -1.f, -1.f}, {1.f, 0.f}},
    { {-1.f, -1.f,  1.f}, {1.f, 1.f}},
    { { 1.f, -1.f,  1.f}, {0.f, 1.f}},
  };

  static constexpr Uint8 INDEX_COUNT = 36;
  static constexpr Uint16 indices[INDEX_COUNT] = {
    // 0, 1, 2,
    // 0, 2, 3,
    0,  1,  2,  0,  2,  3,
    4,  5,  6,  4,  6,  7,
    8,  9,  10, 8,  10, 11,
    12, 13, 14, 12, 14, 15,
    16, 17, 18, 16, 18, 19,
    20, 21, 22, 20, 22, 23
  };
  // clang-format on
};
