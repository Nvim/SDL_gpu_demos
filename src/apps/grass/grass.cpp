#include <SDL3/SDL_gpu.h>
#include <pch.h>

#include "grass.h"

#include "common/compute_pipeline_builder.h"
#include "common/gltf_loader.h"
#include "common/loaded_image.h"
#include "common/logger.h"
#include "common/pipeline_builder.h"
#include "common/types.h"
#include "common/util.h"

#include <glm/ext/matrix_transform.hpp>
#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/backends/imgui_impl_sdlgpu3.h>
#include <imgui/imgui.h>
#include <vector>

namespace grass {
GrassProgram::GrassProgram(SDL_GPUDevice* device,
                           SDL_Window* window,
                           Engine* engine,
                           i32 w,
                           i32 h)
  : Program{ device, window, engine }
  , window_w_{ w }
  , window_h_{ h }
{
  rendertarget_h_ = window_h_ * (3 / 4.f);
  rendertarget_w_ = window_w_ * (3 / 4.f);
  camera_.SetAspect((f32)rendertarget_w_ / (f32)rendertarget_h_);
  camera_.SetNearFar(.1f, 1000.f);
  camera_.Position = { 2.5f, 10.f, 4.5f };
  camera_.Pitch = -13.f;
  camera_.Yaw = -25.f;
  {
    scene_color_target_info_.clear_color = { .85f, .85f, .85f, 1.f };
    scene_color_target_info_.load_op = SDL_GPU_LOADOP_CLEAR;
    scene_color_target_info_.store_op = SDL_GPU_STOREOP_STORE;
  }
  {
    scene_depth_target_info_.cycle = true;
    scene_depth_target_info_.clear_depth = 1;
    scene_depth_target_info_.clear_stencil = 0;
    scene_depth_target_info_.load_op = SDL_GPU_LOADOP_CLEAR;
    scene_depth_target_info_.store_op = SDL_GPU_STOREOP_STORE;
    scene_depth_target_info_.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    scene_depth_target_info_.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
  }

  {
    swapchain_target_info_.clear_color = { .1f, .1f, .1f, .1f };
    swapchain_target_info_.load_op = SDL_GPU_LOADOP_CLEAR;
    swapchain_target_info_.store_op = SDL_GPU_STOREOP_STORE;
    swapchain_target_info_.mip_level = 0;
    swapchain_target_info_.layer_or_depth_plane = 0;
    swapchain_target_info_.cycle = false;
  }
}

GrassProgram::~GrassProgram()
{
  LOG_DEBUG("Destroying GrassProgram");

  RELEASE_IF(grass_pipeline_, SDL_ReleaseGPUGraphicsPipeline);
  RELEASE_IF(generate_grass_pipeline_, SDL_ReleaseGPUComputePipeline);
  RELEASE_IF(cull_chunks_pipeline_, SDL_ReleaseGPUComputePipeline);
  RELEASE_IF(grassblade_indices_, SDL_ReleaseGPUBuffer);
  RELEASE_IF(grassblade_vertices_, SDL_ReleaseGPUBuffer);
  RELEASE_IF(grassblade_instances_, SDL_ReleaseGPUBuffer);

  RELEASE_IF(terrain_pipeline_, SDL_ReleaseGPUGraphicsPipeline);
  RELEASE_IF(chunk_indices_, SDL_ReleaseGPUBuffer);
  RELEASE_IF(chunk_instances_, SDL_ReleaseGPUBuffer);
  RELEASE_IF(visible_chunks_, SDL_ReleaseGPUBuffer);
  RELEASE_IF(draw_calls_, SDL_ReleaseGPUBuffer);

  RELEASE_IF(depth_target_, SDL_ReleaseGPUTexture);
  RELEASE_IF(scene_target_, SDL_ReleaseGPUTexture);
  RELEASE_IF(noise_texture_, SDL_ReleaseGPUTexture);
  SDL_WaitForGPUIdle(Device);
  ImGui_ImplSDL3_Shutdown();
  ImGui_ImplSDLGPU3_Shutdown();
  ImGui::DestroyContext();
}

bool
GrassProgram::Init()
{
  LOG_TRACE("GrassProgram::Init");
  if (!InitGui()) {
    LOG_CRITICAL("Couldn't initialize ImGui");
    return false;
  }
  if (!skybox_.IsLoaded()) {
    LOG_CRITICAL("Couldn't load skybox");
    return false;
  }
  if (!CreateRenderTargets()) {
    LOG_CRITICAL("Couldn't create render targets");
    return false;
  }
  if (!CreateGraphicsPipelines()) {
    LOG_CRITICAL("Couldn't create graphics pipeline");
    return false;
  }
  if (!CreateComputePipelines()) {
    LOG_CRITICAL("Couldn't create compute pipeline");
    return false;
  }

  if (!UploadVertexData()) {
    LOG_CRITICAL("Couldn't create grassblade ssbo & index buffers");
    return false;
  }

  { // noise
    SDL_GPUTextureCreateInfo info{};
    {
      info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
      info.height = 128;
      info.width = 128;
      info.num_levels = 1;
      info.layer_count_or_depth = 1;
      info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    }
    noise_texture_ = SDL_CreateGPUTexture(Device, &info);
    if (!noise_texture_) {
      LOG_CRITICAL("Couldn't create noise texture");
      return false;
    }
    LoadedImage noise_img{};
    if (!ImageLoader::Load(
          noise_img,
          // "resources/textures/noise/128x128/Spokes/Spokes 10 - 128x128.png"
          "resources/textures/noise/128x128/Manifold/Manifold 13 - 128x128.png"
          //
          )) {
      LOG_CRITICAL("Couldn't load noise image");
      return false;
    }

    if (!EnginePtr->UploadTo2DTexture(noise_texture_, noise_img)) {
      LOG_CRITICAL("Couldn't upload to noise texture");
      return false;
    }
    noise_sampler_ = EnginePtr->LinearRepeatSampler();
  }

  if (!GenerateGrassblades()) {
    LOG_CRITICAL("Couldn't create grassblades");
    return false;
  }

  // Indirect draws buffer:
  SDL_GPUBufferCreateInfo buf_info{};
  {
    // NOTE:
    // We only draw 1 instanced quad, so buffer's size should be
    // 1*DrawIndirectCommand. However, the SDL_GPUIndirectDrawCommand is smaller
    // than Vulkan's vkCmdDrawIndexedIndirect, so we clamp to this value to
    // avoid validation error.
    buf_info.size = std::max(20lu, sizeof(SDL_GPUIndirectDrawCommand));
    buf_info.usage = SDL_GPU_BUFFERUSAGE_INDIRECT;
  }
  draw_calls_ = SDL_CreateGPUBuffer(Device, &buf_info);
  if (!draw_calls_) {
    LOG_ERROR("Couldn't create indirect buffer: {}", GETERR);
    return false;
  }

  SDL_GPUIndirectDrawCommand draw_cmd{};
  std::memset(&draw_cmd, 0, sizeof(draw_cmd));
  draw_cmd.num_vertices = 6;

  EnginePtr->UploadToBuffer(draw_calls_, &draw_cmd, 1);

  return true;
}

static u64 prev_grassblades_size = 0;
static u64 prev_chunks_size = 0;
static u64 prev_visible_chunks_size = 0;
bool
GrassProgram::GenerateGrassblades()
{
  LOG_TRACE("GrassProgram::GenerateGrassblades");
  regenerate_grass_ = false;

  auto& p = grass_gen_params_;
  auto blades_per_chunk = p.grass_per_chunk * p.grass_per_chunk;
  auto total_chunks = p.terrain_width * p.terrain_width;

  { // resize storage buffers if needed

    u32 new_grassblades_size =
      blades_per_chunk * total_chunks * GRASS_INSTANCE_SZ;
    u32 new_chunks_size = total_chunks * CHUNK_INSTANCE_SZ;
    u32 new_visible_chunks_size = total_chunks * sizeof(u32);

    const auto resize_buffer =
      [&](SDL_GPUBuffer** buf, u64 new_size, u64& prev_size) -> bool {
      if (new_size != prev_size) {
        LOG_DEBUG(
          "Resizing storage buffer from {} to {} bytes", prev_size, new_size);
        SDL_GPUBufferCreateInfo buf_info{};
        {
          buf_info.size = new_size;
          buf_info.usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE |
                           SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
        }
        auto* ret = SDL_CreateGPUBuffer(Device, &buf_info);
        if (!ret) {
          LOG_ERROR("Couldn't create storage buffer: {}", GETERR);
          return false;
        }
        SDL_ReleaseGPUBuffer(Device, *buf);
        *buf = ret;
        prev_size = new_size;
      } else {
        LOG_DEBUG("Skipped storage buffer re-creation");
      }
      return true;
    };

    if (!resize_buffer(&grassblade_instances_,
                       new_grassblades_size,
                       prev_grassblades_size)) {
      LOG_ERROR("Couldn't resize grassblades buffer");
      return false;
    }
    if (!resize_buffer(&chunk_instances_, new_chunks_size, prev_chunks_size)) {
      LOG_ERROR("Couldn't resize chunks buffer");
      return false;
    }
    if (!resize_buffer(&visible_chunks_,
                       new_visible_chunks_size,
                       prev_visible_chunks_size)) {
      LOG_ERROR("Couldn't resize visible chunks buffers");
      return false;
    }
  }

  { // Dispatch compute
    SDL_GPUStorageBufferReadWriteBinding bindings[2];
    {
      bindings[0].buffer = grassblade_instances_;
      bindings[0].cycle = false;
      bindings[1].buffer = chunk_instances_;
      bindings[1].cycle = false;
    }

    SDL_GPUCommandBuffer* cmd_buf = SDL_AcquireGPUCommandBuffer(Device);
    if (cmd_buf == NULL) {
      LOG_ERROR("Couldn't acquire command buffer: {}", GETERR);
      return false;
    }
    auto* pass = SDL_BeginGPUComputePass(cmd_buf, nullptr, 0, bindings, 2);
    SDL_BindGPUComputePipeline(pass, generate_grass_pipeline_);
    SDL_PushGPUComputeUniformData(
      cmd_buf, 0, &grass_gen_params_, sizeof(GrassGenerationParams));
    SDL_PushGPUComputeUniformData(cmd_buf, 1, &lastTime, sizeof(lastTime));
    SDL_DispatchGPUCompute(pass, p.terrain_width, p.terrain_width, 1);
    SDL_EndGPUComputePass(pass);
    if (!SDL_SubmitGPUCommandBuffer(cmd_buf)) {
      LOG_ERROR("Instance buffer generation failed: {}", GETERR);
      return false;
    }
  }

  return true;
}

static u32 cull_dispatch_sz{ 0 };
bool
GrassProgram::CullChunks(CameraBinding& camera)
{
  SDL_GPUStorageBufferReadWriteBinding bindings[2];
  {
    bindings[0].buffer = visible_chunks_;
    bindings[0].cycle = false;
    bindings[1].buffer = draw_calls_;
    bindings[1].cycle = false;
  }

  SDL_GPUCommandBuffer* cmd_buf = SDL_AcquireGPUCommandBuffer(Device);
  if (cmd_buf == NULL) {
    LOG_ERROR("Couldn't acquire command buffer: {}", GETERR);
    return false;
  }
  auto* pass = SDL_BeginGPUComputePass(cmd_buf, nullptr, 0, bindings, 2);
  SDL_BindGPUComputePipeline(pass, cull_chunks_pipeline_);

  SDL_BindGPUComputeStorageBuffers(pass, 0, &chunk_instances_, 1);
  SDL_PushGPUComputeUniformData(
    cmd_buf, 0, &terrain_params_, sizeof(terrain_params_));
  SDL_PushGPUComputeUniformData(cmd_buf, 1, &camera, sizeof(camera));

  static constexpr f32 local_size = 16; // match in shader
  // const u32
  cull_dispatch_sz = std::ceil(grass_gen_params_.terrain_width / local_size);
  SDL_DispatchGPUCompute(pass, cull_dispatch_sz, cull_dispatch_sz, 1);

  SDL_EndGPUComputePass(pass);
  if (!SDL_SubmitGPUCommandBuffer(cmd_buf)) {
    LOG_ERROR("Instance buffer generation failed: {}", GETERR);
    return false;
  }
  return true;
}

bool
GrassProgram::Poll()
{
  SDL_Event evt;
  while (SDL_PollEvent(&evt)) {
    ImGui_ImplSDL3_ProcessEvent(&evt);
    if (evt.type == SDL_EVENT_QUIT) {
      quit_ = true;
    } else if (evt.type == SDL_EVENT_KEY_DOWN) {
      if (evt.key.key == SDLK_ESCAPE) {
        quit_ = true;
      }
    }
  }
  camera_.Update(DeltaTime);
  if (regenerate_grass_) {
    if (!GenerateGrassblades()) {
      LOG_ERROR("Couldn't regenerate grassblades");
    }
  }
  return true;
}

bool
GrassProgram::Draw()
{
  static const SDL_GPUViewport scene_vp{
    0, 0, float(rendertarget_w_), float(rendertarget_h_), 0.1f, 1.0f
  };

  SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(Device);
  if (cmdbuf == NULL) {
    LOG_ERROR("Couldn't acquire command buffer: {}", SDL_GetError());
    return false;
  }

  SDL_GPUTexture* swapchain_tex;
  if (!SDL_WaitAndAcquireGPUSwapchainTexture(
        cmdbuf, Window, &swapchain_tex, NULL, NULL)) {
    LOG_ERROR("Couldn't acquire swapchain texture: {}", SDL_GetError());
    return false;
  }
  if (swapchain_tex == NULL) {
    SDL_SubmitGPUCommandBuffer(cmdbuf);
    return true;
  }

  auto draw_data = DrawGui();
  ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, cmdbuf);

  { // Scene pass
    SDL_GPURenderPass* scene_pass = SDL_BeginGPURenderPass(
      cmdbuf, &scene_color_target_info_, 1, &scene_depth_target_info_);

    CameraBinding camera_bind{
      .viewproj = camera_.Projection() * camera_.View(),
      .camera_model = camera_.Model(),
      .camera_world = glm::vec4{ camera_.Position, 1.f },
    };
    static CameraBinding cull_camera_bind{ camera_bind };
    if (!freeze_cull_camera) {
      cull_camera_bind = camera_bind;
    }

    SDL_SetGPUViewport(scene_pass, &scene_vp);

    if (true) {
      CullChunks(cull_camera_bind);
    }
    if (draw_grass_) {
      DrawGrass(scene_pass, cmdbuf, camera_bind);
    }
    if (draw_terrain_) {
      DrawTerrain(scene_pass, cmdbuf, camera_bind);
    }

    skybox_.Draw(cmdbuf, scene_pass, camera_bind);

    SDL_EndGPURenderPass(scene_pass);
  }

  { // Fullscreen pass
    swapchain_target_info_.texture = swapchain_tex;
    SDL_GPURenderPass* screen_pass =
      SDL_BeginGPURenderPass(cmdbuf, &swapchain_target_info_, 1, nullptr);

    ImGui_ImplSDLGPU3_RenderDrawData(draw_data, cmdbuf, screen_pass);
    SDL_EndGPURenderPass(screen_pass);
  }

  SDL_SubmitGPUCommandBuffer(cmdbuf);
  // auto fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmdbuf);
  // if (!fence) {
  //   LOG_ERROR("couldn't acquire submit fence: {}", GETERR);
  //   return false;
  // }
  // SDL_WaitForGPUFences(Device, true, &fence, 1);
  // SDL_GPUIndirectDrawCommand draw_cmd{};
  // std::memset(&draw_cmd, 0, sizeof(draw_cmd));
  // draw_cmd.num_vertices = 6;
  // if (!EnginePtr->UploadToBuffer(draw_calls_, &draw_cmd, 1)) {
  //   LOG_ERROR("couldn't reset draw buffer: {}", GETERR);
  //   return false;
  // }
  return true;
}

void
GrassProgram::DrawGrass(SDL_GPURenderPass* pass,
                        SDL_GPUCommandBuffer* cmdbuf,
                        CameraBinding& camera)
{
  static const SDL_GPUTextureSamplerBinding b{ noise_texture_, noise_sampler_ };
  static const SDL_GPUBufferBinding grass_idx_bind{ grassblade_indices_, 0 };

  SDL_BindGPUGraphicsPipeline(pass, grass_pipeline_);

  SDL_BindGPUVertexSamplers(pass, 0, &b, 1);
  SDL_BindGPUVertexStorageBuffers(pass, 0, &grassblade_vertices_, 1);
  SDL_BindGPUVertexStorageBuffers(pass, 1, &grassblade_instances_, 1);
  SDL_BindGPUVertexStorageBuffers(pass, 2, &chunk_instances_, 1);
  SDL_PushGPUVertexUniformData(cmdbuf, 0, &camera, sizeof(camera));
  SDL_PushGPUVertexUniformData(
    cmdbuf, 1, &terrain_params_, sizeof(terrain_params_));

  SDL_PushGPUFragmentUniformData(cmdbuf, 0, &sunlight_, sizeof(sunlight_));
  SDL_PushGPUFragmentUniformData(
    cmdbuf, 1, &grass_gen_params_.base_color, sizeof(glm::vec3));
  SDL_PushGPUFragmentUniformData(
    cmdbuf, 2, &fog_settings, sizeof(fog_settings));

  SDL_BindGPUIndexBuffer(pass, &grass_idx_bind, SDL_GPU_INDEXELEMENTSIZE_32BIT);

  auto& p = grass_gen_params_;
  auto blades_per_chunk = p.grass_per_chunk * p.grass_per_chunk;
  auto total_chunks = p.terrain_width * p.terrain_width;
  SDL_DrawGPUIndexedPrimitives(
    pass, grassblade_index_count_, total_chunks * blades_per_chunk, 0, 0, 0);
}

void
GrassProgram::DrawTerrain(SDL_GPURenderPass* pass,
                          SDL_GPUCommandBuffer* cmdbuf,
                          CameraBinding& camera)
{
  static const SDL_GPUTextureSamplerBinding b{ noise_texture_, noise_sampler_ };
  static const SDL_GPUBufferBinding chunk_idx_bind{ chunk_indices_, 0 };

  SDL_BindGPUGraphicsPipeline(pass, terrain_pipeline_);

  SDL_BindGPUVertexSamplers(pass, 0, &b, 1);
  SDL_BindGPUVertexStorageBuffers(pass, 0, &chunk_instances_, 1);
  SDL_BindGPUVertexStorageBuffers(pass, 1, &visible_chunks_, 1);
  SDL_PushGPUVertexUniformData(cmdbuf, 0, &camera, sizeof(camera));
  SDL_PushGPUVertexUniformData(
    cmdbuf, 1, &terrain_params_, sizeof(terrain_params_));
  SDL_PushGPUFragmentUniformData(
    cmdbuf, 0, &fog_settings, sizeof(fog_settings));

  SDL_BindGPUIndexBuffer(pass, &chunk_idx_bind, SDL_GPU_INDEXELEMENTSIZE_32BIT);

  auto& p = grass_gen_params_;
  auto total_chunks = p.terrain_width * p.terrain_width;
  // SDL_DrawGPUIndexedPrimitives(pass, 6, total_chunks, 0, 0, 0);
  SDL_DrawGPUIndexedPrimitivesIndirect(pass, draw_calls_, 0, 1);
}

bool
GrassProgram::ShouldQuit()
{
  return quit_;
}

bool
GrassProgram::InitGui()
{
  LOG_TRACE("GrassProgram::InitGui");
  float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |=
    ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
  io.ConfigFlags |=
    ImGuiConfigFlags_NavEnableGamepad;              // Enable Gamepad Controls
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable Docking
  io.IniFilename = "imgui/grass.ini";

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();
  // ImGui::StyleColorsLight();

  // Setup scaling
  ImGuiStyle& style = ImGui::GetStyle();
  style.ScaleAllSizes(main_scale);
  style.FontScaleDpi = main_scale;

  // Setup Platform/Renderer backends
  if (!ImGui_ImplSDL3_InitForSDLGPU(Window)) {
    return false;
  };
  ImGui_ImplSDLGPU3_InitInfo init_info = {};
  init_info.Device = Device;
  init_info.ColorTargetFormat =
    SDL_GetGPUSwapchainTextureFormat(Device, Window);
  init_info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
  init_info.SwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
  init_info.PresentMode = SDL_GPU_PRESENTMODE_VSYNC;
  return ImGui_ImplSDLGPU3_Init(&init_info);
}

bool
GrassProgram::CreateRenderTargets()
{
  LOG_TRACE("GrassProgram::CreateSceneRenderTargets");
  auto info = SDL_GPUTextureCreateInfo{};

  { // Scene pass target
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = TARGET_FORMAT;
    info.width = static_cast<Uint32>(rendertarget_w_);
    info.height = static_cast<Uint32>(rendertarget_h_);
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    info.usage =
      SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    scene_target_ = SDL_CreateGPUTexture(Device, &info);
    if (!scene_target_) {
      LOG_ERROR("Couldn't create scene rendertarget: {}", GETERR);
      return false;
    }
  }

  { // Depth target
    info.format = DEPTH_FORMAT;
    info.usage =
      SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    depth_target_ = SDL_CreateGPUTexture(Device, &info);
    if (!depth_target_) {
      LOG_ERROR("Couldn't create depth rendertarget: {}", GETERR);
      return false;
    }
  }

  scene_color_target_info_.texture = scene_target_;
  scene_depth_target_info_.texture = depth_target_;

  return true;
}

bool
GrassProgram::CreateGraphicsPipelines()
{
  LOG_TRACE("GrassProgram::CreateGraphicsPipelines");

  PipelineBuilder builder;
  builder //
    .AddColorTarget(TARGET_FORMAT, false)
    .SetPrimitiveType(SDL_GPU_PRIMITIVETYPE_TRIANGLELIST)
    .EnableDepthTest()
    .SetCompareOp(SDL_GPU_COMPAREOP_LESS)
    .EnableDepthWrite(DEPTH_FORMAT);

  { // Grass
    auto vert = LoadShader(GRASS_VS_PATH, Device, 1, 2, 3, 0);
    if (vert == nullptr) {
      LOG_ERROR("Couldn't load vertex shader at path {}", GRASS_VS_PATH);
      return false;
    }
    auto frag = LoadShader(GRASS_FS_PATH, Device, 0, 3, 0, 0);
    if (frag == nullptr) {
      LOG_ERROR("Couldn't load fragment shader at path {}", GRASS_FS_PATH);
      return false;
    }
    grass_pipeline_ = builder //
                        .SetVertexShader(vert)
                        .SetFragmentShader(frag)
                        .Build(Device);

    if (!grass_pipeline_) {
      LOG_ERROR("Couldn't build pipeline: {}", GETERR);
      return false;
    }

    SDL_ReleaseGPUShader(Device, frag);
    SDL_ReleaseGPUShader(Device, vert);
  }

  { // Terrain
    auto vert = LoadShader(TERRAIN_VS_PATH, Device, 1, 2, 2, 0);
    if (vert == nullptr) {
      LOG_ERROR("Couldn't load vertex shader at path {}", TERRAIN_VS_PATH);
      return false;
    }
    auto frag = LoadShader(TERRAIN_FS_PATH, Device, 0, 1, 0, 0);
    if (frag == nullptr) {
      LOG_ERROR("Couldn't load fragment shader at path {}", TERRAIN_FS_PATH);
      return false;
    }
    terrain_pipeline_ = builder //
                          .SetVertexShader(vert)
                          .SetFragmentShader(frag)
                          .Build(Device);
    if (!terrain_pipeline_) {
      LOG_ERROR("Couldn't build pipeline: {}", GETERR);
      return false;
    }

    SDL_ReleaseGPUShader(Device, frag);
    SDL_ReleaseGPUShader(Device, vert);
  }
  return true;
}

bool
GrassProgram::CreateComputePipelines()
{
  LOG_TRACE("CubeProgram::CreateComputePipeline");

  ComputePipelineBuilder builder{};
  const auto tcount = grass_gen_params_.grass_per_chunk;
  generate_grass_pipeline_ = builder //
                               .SetReadOnlyStorageTextureCount(0)
                               .SetReadWriteStorageTextureCount(0)
                               .SetReadWriteStorageBufferCount(2)
                               .SetUBOCount(2)
                               .SetThreadCount(tcount, tcount, 1)
                               .SetShader(COMP_PATH)
                               .Build(Device);

  if (generate_grass_pipeline_ == nullptr) {
    LOG_ERROR("Couldn't create generate_grass pipeline: {}", GETERR);
    return false;
  }

  cull_chunks_pipeline_ = builder                             //
                            .SetReadOnlyStorageBufferCount(1) // Read chunks
                            .SetReadWriteStorageBufferCount(
                              2) // Write visible chunks + Indirect buffer
                            .SetUBOCount(2)
                            .SetShader(CULL_COMP_PATH)
                            .Build(Device);

  if (cull_chunks_pipeline_ == nullptr) {
    LOG_ERROR("Couldn't create cull_chunks pipeline: {}", GETERR);
    return false;
  }

  return true;
}

bool
GrassProgram::UploadVertexData()
{
  LOG_TRACE("GrassProgram::UploadVertexData");
  std::vector<PosNormalVertex_Aligned> vertices{};
  std::vector<u32> indices{};
  constexpr std::array<u32, 6> chunk_idx{ 0, 1, 2, 1, 3, 2 };

  if (!GLTFLoader::LoadPositions(GRASS_PATH, vertices, indices, 0)) {
    LOG_ERROR("Couldn't load vertex data");
    return false;
  }

  auto vert_count = vertices.size();
  grassblade_index_count_ = indices.size();
  LOG_DEBUG("Grassblade has {} vertices and {} indices",
            vert_count,
            grassblade_index_count_);
  {
    SDL_GPUBufferCreateInfo idxInfo{};
    {
      idxInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX;
      idxInfo.size = static_cast<u32>(sizeof(u32) * grassblade_index_count_);
    }
    grassblade_indices_ = SDL_CreateGPUBuffer(Device, &idxInfo);
    if (!grassblade_indices_) {
      LOG_ERROR("couldn't create grassblades index buffer");
      return false;
    }

    idxInfo.size = 6 * sizeof(u32);
    chunk_indices_ = SDL_CreateGPUBuffer(Device, &idxInfo);
    if (!chunk_indices_) {
      LOG_ERROR("couldn't create chunks index buffer");
      return false;
    }
  }
  {
    SDL_GPUBufferCreateInfo ssbo_info{};
    {
      ssbo_info.usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
      ssbo_info.size =
        static_cast<u32>(sizeof(PosNormalVertex_Aligned) * vert_count);
    }
    grassblade_vertices_ = SDL_CreateGPUBuffer(Device, &ssbo_info);
    if (!grassblade_vertices_) {
      LOG_ERROR("couldn't create vertices ssbo");
      return false;
    }
  }
  if (!EnginePtr->UploadToBuffer(
        grassblade_indices_, indices.data(), grassblade_index_count_)) {
    LOG_ERROR("Couldn't upload index buffer data");
    return false;
  }
  if (!EnginePtr->UploadToBuffer(
        grassblade_vertices_, vertices.data(), vert_count)) {
    LOG_ERROR("Couldn't upload vertices ssbo data");
    return false;
  }
  if (!EnginePtr->UploadToBuffer(chunk_indices_, chunk_idx.data(), 6)) {
    LOG_ERROR("Couldn't upload index buffer data");
    return false;
  }
  return true;
}

static constexpr Flag grass_flags[] = {
  { "Offset positions", GRASS_OFFSET_POS },
  { "Random rotation", GRASS_ROTATE },
};

static constexpr Flag fog_flags[] = {
  { "Off", FOG_NONE },
  { "Linear", FOG_LINEAR },
  { "Exponential", FOG_EXP },
  { "Exponential Squared", FOG_EXP_SQ },
};

ImDrawData*
GrassProgram::DrawGui()
{
  // Init frame:
  {
    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
  }

  if (ImGui::Begin("Stats")) {
    auto& p = grass_gen_params_;
    ImGui::Text("Total grassblades: %d",
                p.grass_per_chunk * p.grass_per_chunk * p.terrain_width *
                  p.terrain_width);
    ImGui::Text("Total chunks: %d", p.terrain_width * p.terrain_width);
    ImGui::Text(
      "Cull workgroup size: %ux%u", cull_dispatch_sz, cull_dispatch_sz);
    ImGui::Text("Grassblades buffer size: %lu", prev_grassblades_size);
    ImGui::Text("Chunks buffer size: %lu", prev_chunks_size);
    ImGui::Text("Visible chunks buffer size: %lu", prev_visible_chunks_size);
    ImGui::End();
  }
  if (ImGui::Begin("Settings")) {
    ImGui::Checkbox("Freeze culling camera", &freeze_cull_camera);
    if (ImGui::TreeNode("Viewport")) {
      ImGui::Text("Window Width: %d", window_w_);
      ImGui::Text("Window Height: %d", window_h_);
      ImGui::Text("Rendertarget Width: %d", rendertarget_w_);
      ImGui::Text("Rendertarget Height: %d", rendertarget_h_);
      ImGui::Text("Aspect Ratio: %f",
                  (f32)rendertarget_w_ / (f32)rendertarget_h_);
      ImGui::TreePop();
    }
    if (ImGui::TreeNode("Camera")) {
      ImGui::Text("Position");
      if (ImGui::SliderFloat3(
            "Translation", (f32*)&camera_.Position, -150.f, 150.f)) {
        camera_.Moved = true;
      }
      ImGui::Text("Rotation");
      if (ImGui::SliderFloat("Yaw", (float*)&camera_.Yaw, -90.f, 90.f) ||
          ImGui::SliderFloat("Pitch", (float*)&camera_.Pitch, -90.f, 90.f)) {
        camera_.Rotated = true;
      }
      ImGui::TreePop();
    }
    if (ImGui::TreeNode("Lighting")) {
      ImGui::Text("Sun:");
      ImGui::SliderFloat3("Position:", (f32*)&sunlight_.direction, 1.f, 50.f);
      ImGui::ColorEdit3("Ambient:", (f32*)&sunlight_.ambient);
      ImGui::ColorEdit3(
        "Diffuse:", (f32*)&sunlight_.diffuse, ImGuiColorEditFlags_Float);
      ImGui::ColorEdit3("Specular:", (f32*)&sunlight_.specular);
      ImGui::TreePop();
    }
    if (ImGui::TreeNode("Terrain")) {
      auto& tp = terrain_params_;
      ImGui::Checkbox("Draw terrain", &draw_terrain_);
      ImGui::Checkbox("Draw grass", &draw_grass_);
      ImGui::Checkbox("Highlight chunks", (bool*)&tp.highlight_chunks);
      ImGui::SliderInt("World size", &tp.world_size, 1, 256);
      ImGui::SliderFloat("Heightmap scale", &tp.heightmap_scale, 1.f, 128.f);
      ImGui::ColorEdit3("Color:", (f32*)&tp.terrain_color);
      ImGui::TreePop();
    }
    if (ImGui::TreeNode("Grass Generation")) {
      static auto tmp_params = grass_gen_params_; // copy to apply all at once
      ImGui::ColorEdit3("Base Color:", (f32*)&tmp_params.base_color);
      ImGui::SliderInt("Chunk count", (i32*)&tmp_params.terrain_width, 4, 64);
      ImGui::SliderInt(
        "Grassblades per chunk", (i32*)&tmp_params.grass_per_chunk, 1.f, 32.f);
      ImGui::SliderFloat("Density", (f32*)&tmp_params.density, 1.f, 20.f);

      for (const Flag& flag : grass_flags) {
        bool flag_set = (tmp_params.flags & flag.flag_value) != 0;
        if (ImGui::Checkbox(flag.label, &flag_set)) {
          if (flag_set) {
            tmp_params.flags |= flag.flag_value;
          } else {
            tmp_params.flags &= ~flag.flag_value;
          }
        }
      }
      ImGui::SliderFloat(
        "Max position offset", (f32*)&tmp_params.offset_cap, 0.f, 1.f);

      if (ImGui::Button("Generate")) {
        grass_gen_params_ = tmp_params;
        terrain_params_.terrain_width = grass_gen_params_.terrain_width;
        regenerate_grass_ = true;
      }

      ImGui::SameLine();
      if (ImGui::Button("Discard changes")) {
        tmp_params = grass_gen_params_;
      }

      ImGui::TreePop();
    }
    if (ImGui::TreeNode("Fog")) {
      ImGui::ColorEdit3("Color", (f32*)&fog_settings.fog_color);
      ImGui::SliderFloat("Density", &fog_settings.fog_density, 0.f, 2.f);
      ImGui::SliderFloat("Max Distance", &fog_settings.fog_end, 10.f, 100.f);
      ImGui::SliderFloat("Begin Distance", &fog_settings.fog_start, 1.f, 100.f);
      for (const Flag& flag : fog_flags) {
        bool flag_set = (fog_settings.fog_type & flag.flag_value) != 0;
        if (ImGui::Checkbox(flag.label, &flag_set)) {
          if (flag_set) {
            fog_settings.fog_type = flag.flag_value;
          } else {
            // fog_settings.fog_type &= ~flag.flag_value;
          }
        }
      }
      ImGui::TreePop();
    }
    ImGui::End();
  }
  if (ImGui::Begin("Scene")) {
    ImGui::Image((ImTextureID)(intptr_t)scene_target_,
                 ImVec2((float)rendertarget_w_, (float)rendertarget_h_));
    ImGui::End();
  }

  ImGui::Render();
  return ImGui::GetDrawData();
}
} // namespace grass
