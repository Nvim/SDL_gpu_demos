#include "cube.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_stdinc.h>
#include <cassert>
#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/backends/imgui_impl_sdlgpu3.h>
#include <imgui/imgui.h>

#include <glm/common.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <vector>

#include "src/camera.h"
#include "src/logger.h"
#include "util.h"

CubeProgram::CubeProgram(SDL_GPUDevice* device,
                         SDL_Window* window,
                         const char* vertex_path,
                         const char* fragment_path,
                         int w,
                         int h)
  : Program{ device, window }
  , vertex_path_{ vertex_path }
  , fragment_path_{ fragment_path }
  , vp_width_{ w }
  , vp_height_{ h }
{
  {
    scene_color_target_info_.clear_color = { 0.1f, 0.1f, 0.1f, 1.0f };
    scene_color_target_info_.load_op = SDL_GPU_LOADOP_CLEAR;
    scene_color_target_info_.store_op = SDL_GPU_STOREOP_STORE;
  }

  {
    scene_depth_target_info_.cycle = true;
    scene_depth_target_info_.clear_depth = 1;
    scene_depth_target_info_.clear_stencil = 0;
    scene_depth_target_info_.load_op = SDL_GPU_LOADOP_CLEAR;
    scene_depth_target_info_.store_op = SDL_GPU_STOREOP_STORE;
    scene_depth_target_info_.stencil_load_op = SDL_GPU_LOADOP_CLEAR;
    scene_depth_target_info_.stencil_store_op = SDL_GPU_STOREOP_STORE;
  }

  {
    swapchain_target_info_.clear_color = { .1f, .1f, .1f, .1f };
    swapchain_target_info_.load_op = SDL_GPU_LOADOP_CLEAR;
    swapchain_target_info_.store_op = SDL_GPU_STOREOP_STORE;
    swapchain_target_info_.mip_level = 0;
    swapchain_target_info_.layer_or_depth_plane = 0;
    swapchain_target_info_.cycle = false;
  }

  {
    rotations_[0] = Rotation{
      "X Axis",
      &cube_transform_.rotation_.x,
      0.f,
    };
    rotations_[1] = Rotation{
      "Y Axis",
      &cube_transform_.rotation_.y,
      1.f,
    };
    rotations_[2] = Rotation{
      "Z Axis",
      &cube_transform_.rotation_.z,
      0.f,
    };
  }
}

CubeProgram::~CubeProgram()
{
  LOG_TRACE("Destroying app");

  // TODO: segfaults:
  // loader_.Release();
  RELEASE_IF(vertex_, SDL_ReleaseGPUShader);
  RELEASE_IF(fragment_, SDL_ReleaseGPUShader);
  RELEASE_IF(scene_pipeline_, SDL_ReleaseGPUGraphicsPipeline);
  RELEASE_IF(scene_wireframe_pipeline_, SDL_ReleaseGPUGraphicsPipeline);
  RELEASE_IF(depth_target_, SDL_ReleaseGPUTexture);
  RELEASE_IF(color_target_, SDL_ReleaseGPUTexture);
  RELEASE_IF(vbuffer_, SDL_ReleaseGPUBuffer);
  RELEASE_IF(ibuffer_, SDL_ReleaseGPUBuffer);

  LOG_DEBUG("Released GPU Resources");

  SDL_WaitForGPUIdle(Device);
  ImGui_ImplSDL3_Shutdown();
  ImGui_ImplSDLGPU3_Shutdown();
  ImGui::DestroyContext();
}

bool
CubeProgram::Init()
{
  LOG_TRACE("CubeProgram::Init");
  if (!InitGui()) {
    LOG_ERROR("Couldn't init imgui");
    return false;
  }
  LOG_DEBUG("Started ImGui");
  SDL_GPUShaderFormat backendFormats = SDL_GetGPUShaderFormats(Device);
  if (!(backendFormats & SDL_GPU_SHADERFORMAT_SPIRV)) {
    LOG_ERROR("Backend doesn't support SPRIR-V");
    return false;
  }

  if (!LoadShaders()) {
    LOG_ERROR("Couldn't load shaders");
    return false;
  }
  LOG_DEBUG("Loaded shaders");

  SDL_GPUColorTargetDescription color_descs[1]{};
  color_descs[0].format = SDL_GetGPUSwapchainTextureFormat(Device, Window);

  SDL_GPUVertexAttribute vertex_attributes[] = {
    { .location = 0,
      .buffer_slot = 0,
      .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
      .offset = 0 },
    { .location = 1,
      .buffer_slot = 0,
      .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
      .offset = sizeof(float) * 3 }
  };

  SDL_GPUVertexBufferDescription vertex_desc[] = { {
    .slot = 0,
    .pitch = sizeof(PosUvVertex),
    .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
    .instance_step_rate = 0,
  } };

  SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo{};
  {
    pipelineCreateInfo.vertex_shader = vertex_;
    pipelineCreateInfo.fragment_shader = fragment_;
    pipelineCreateInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    {
      auto& state = pipelineCreateInfo.vertex_input_state;
      state.vertex_buffer_descriptions = vertex_desc;
      state.num_vertex_buffers = 1, state.vertex_attributes = vertex_attributes;
      state.num_vertex_attributes = 2;
    }
    {
      auto& state = pipelineCreateInfo.rasterizer_state;
      state.fill_mode = SDL_GPU_FILLMODE_FILL,
      state.cull_mode = SDL_GPU_CULLMODE_NONE;
      state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    }
    {
      auto& state = pipelineCreateInfo.depth_stencil_state;
      state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
      state.write_mask = 0xFF;
      state.enable_depth_test = true;
      state.enable_depth_write = true;
      state.enable_stencil_test = false;
    }
    {
      auto& info = pipelineCreateInfo.target_info;
      info.color_target_descriptions = color_descs;
      info.num_color_targets = 1;
      info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
      info.has_depth_stencil_target = true;
    }
  }

  scene_pipeline_ = SDL_CreateGPUGraphicsPipeline(Device, &pipelineCreateInfo);
  if (scene_pipeline_ == NULL) {
    LOG_ERROR("Couldn't create pipeline!");
    return false;
  }
  pipelineCreateInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_LINE;
  scene_wireframe_pipeline_ =
    SDL_CreateGPUGraphicsPipeline(Device, &pipelineCreateInfo);
  if (scene_wireframe_pipeline_ == NULL) {
    LOG_ERROR("Couldn't create wireframe pipeline!");
    return false;
  }
  LOG_DEBUG("Created pipelines");

  if (!loader_.Load()) {
    LOG_CRITICAL("Couldn't initialize GLTF loader");
    return false;
  }
  LOG_INFO("Loaded {} meshes", loader_.Meshes().size());
  assert(!loader_.Meshes().empty());

  if (!SendVertexData()) {
    LOG_ERROR("Couldn't send vertex data!");
    return false;
  }
  LOG_DEBUG("Sent vertex data to GPU");

  if (!LoadTextures()) {
    LOG_ERROR("Couldn't load textures!");
    return false;
  }
  LOG_DEBUG("Loaded textures");

  if (!CreateSceneRenderTargets()) {
    LOG_ERROR("Couldn't create render target textures!");
    return false;
  }
  LOG_DEBUG("Created render target textures");

  cube_transform_.translation_ = { 0.f, 0.f, 0.0f };
  cube_transform_.scale_ = { 12.f, 12.f, 12.f };

  camera_.Position = glm::vec3{ 0.f, 1.f, -4.f };
  camera_.Target = glm::vec3{ 0.f, 0.f, 0.f };

  LOG_INFO("Initialized application");
  return true;
}

bool
CubeProgram::ShouldQuit()
{
  return quit;
}

bool
CubeProgram::Poll()
{
  SDL_Event evt;

  while (SDL_PollEvent(&evt)) {
    ImGui_ImplSDL3_ProcessEvent(&evt);
    if (evt.type == SDL_EVENT_QUIT) {
      quit = true;
    } else if (evt.type == SDL_EVENT_KEY_DOWN) {
      if (evt.key.key == SDLK_ESCAPE) {
        quit = true;
      }
    }
  }
  return true;
}

void
CubeProgram::UpdateScene()
{
  // static float c = -1.f;

  for (const auto& rot : rotations_) {
    if (rot.speed != 0.f) {
      *rot.axis =
        glm::mod(*rot.axis + DeltaTime * rot.speed, glm::two_pi<float>());
    }
  }

  // if (camera_.Position.z < -10.f) {
  //   c = 1.f;
  // } else if (camera_.Position.z > -1.f) {
  //   c = -1.f;
  // }
  // camera_.Position.z += c * DeltaTime * 2.5f;
  // camera_.Touched = true;
  cube_transform_.Touched = true;
  camera_.Update();
}

bool
CubeProgram::Draw()
{
  static const SDL_GPUViewport scene_vp{
    0, 0, float(vp_width_), float(vp_height_), 0.1f, 1.0f
  };
  static const SDL_GPUBufferBinding vBinding{ vbuffer_, 0 };
  static const SDL_GPUBufferBinding iBinding{ ibuffer_, 0 };

  SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(Device);
  if (cmdbuf == NULL) {
    LOG_ERROR("Couldn't acquire command buffer: {}", SDL_GetError());
    return false;
  }

  SDL_GPUTexture* swapchainTexture;
  if (!SDL_WaitAndAcquireGPUSwapchainTexture(
        cmdbuf, Window, &swapchainTexture, NULL, NULL)) {
    LOG_ERROR("Couldn't acquire swapchain texture: {}", SDL_GetError());
    return false;
  }
  if (swapchainTexture == NULL) {
    SDL_SubmitGPUCommandBuffer(cmdbuf);
    return true;
  }

  UpdateScene(); // TODO: move out
  auto mats = loader_.Materials();
  assert(!mats.empty() && mats[0] != nullptr);
  auto tx = mats[0]->MetalRoughTexture;
  auto s = mats[0]->MetalRoughSampler;
  assert(tx && s);
  SDL_GPUTextureSamplerBinding sampler_bind{ tx, s };
  auto vp = camera_.Projection() * camera_.View();
  MatricesBinding mvp{ vp, cube_transform_.Matrix() };
  auto cameraModel = camera_.Model();
  auto draw_data = DrawGui();
  auto d = instance_cfg.dimension;
  auto total_instances = d * d * d;
  auto& mesh = loader_.Meshes()[0];
  auto idx_count = mesh.indices_.size();

  ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, cmdbuf);

  // Scene Pass
  {
    scene_color_target_info_.texture = color_target_;
    scene_depth_target_info_.texture = depth_target_;
    SDL_PushGPUVertexUniformData(cmdbuf, 0, &mvp, sizeof(mvp));
    SDL_PushGPUVertexUniformData(cmdbuf, 1, &cameraModel, sizeof(cameraModel));
    SDL_PushGPUVertexUniformData(
      cmdbuf, 2, &instance_cfg, sizeof(instance_cfg));

    SDL_GPURenderPass* scenePass = SDL_BeginGPURenderPass(
      cmdbuf, &scene_color_target_info_, 1, &scene_depth_target_info_);

    SDL_SetGPUViewport(scenePass, &scene_vp);

    SDL_BindGPUGraphicsPipeline(
      scenePass, wireframe_ ? scene_wireframe_pipeline_ : scene_pipeline_);
    SDL_BindGPUVertexBuffers(scenePass, 0, &vBinding, 1);
    SDL_BindGPUIndexBuffer(
      scenePass, &iBinding, SDL_GPU_INDEXELEMENTSIZE_16BIT);
    SDL_BindGPUFragmentSamplers(scenePass, 0, &sampler_bind, 1);
    SDL_DrawGPUIndexedPrimitives(
      scenePass, idx_count, total_instances, 0, 0, 0);

    skybox_.Draw(scenePass);

    SDL_EndGPURenderPass(scenePass);
  }

  // GUI Pass
  {
    swapchain_target_info_.texture = swapchainTexture;
    SDL_GPURenderPass* guiPass =
      SDL_BeginGPURenderPass(cmdbuf, &swapchain_target_info_, 1, nullptr);

    ImGui_ImplSDLGPU3_RenderDrawData(draw_data, cmdbuf, guiPass);
    SDL_EndGPURenderPass(guiPass);
  }

  SDL_SubmitGPUCommandBuffer(cmdbuf);
  return true;
}

bool
CubeProgram::LoadShaders()
{
  LOG_TRACE("CubeProgram::LoadShaders");
  vertex_ = LoadShader(vertex_path_, Device, 0, 3, 0, 0);
  if (vertex_ == nullptr) {
    LOG_ERROR("Couldn't load vertex shader at path {}", vertex_path_);
    return false;
  }
  fragment_ = LoadShader(fragment_path_, Device, 1, 1, 0, 0);
  if (fragment_ == nullptr) {
    LOG_ERROR("Couldn't load fragment shader at path {}", fragment_path_);
    return false;
  }
  return true;
}

bool
CubeProgram::LoadTextures()
{
  LOG_TRACE("CubeProgram::LoadTextures");
  // auto img = LoadImage("resources/textures/grass.png");
  // auto img = loader.Surfaces()[0];
  // if (!img) {
  //   LOG_ERROR("Couldn't load images");
  //   return false;
  // }
  // samplers_ = std::vector<SDL_GPUSampler*>{};
  // textures_ = std::vector<SDL_GPUTexture*>{};
  // SDL_GPUSamplerCreateInfo sampler_info{};
  // {
  //   sampler_info.min_filter = SDL_GPU_FILTER_NEAREST;
  //   sampler_info.mag_filter = SDL_GPU_FILTER_NEAREST;
  //   sampler_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
  //   sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  //   sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  //   sampler_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  // }
  // samplers_.push_back(SDL_CreateGPUSampler(Device, &sampler_info));
  //
  // SDL_GPUTextureCreateInfo tex_info{};
  // {
  //   tex_info.type = SDL_GPU_TEXTURETYPE_2D;
  //   tex_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  //   tex_info.width = static_cast<Uint32>(img->w);
  //   tex_info.height = static_cast<Uint32>(img->h);
  //   tex_info.layer_count_or_depth = 1;
  //   tex_info.num_levels = 1;
  //   tex_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
  // }
  // textures_.push_back(SDL_CreateGPUTexture(Device, &tex_info));
  //
  // SDL_GPUTransferBufferCreateInfo tr_info{};
  // {
  //   tr_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  //   tr_info.size = (Uint32)img->w * (Uint32)img->h * 4;
  // };
  // SDL_GPUTransferBuffer* trBuf = SDL_CreateGPUTransferBuffer(Device,
  // &tr_info);
  //
  // void* textureTransferPtr = SDL_MapGPUTransferBuffer(Device, trBuf, false);
  // SDL_memcpy(textureTransferPtr, img->pixels, img->w * img->h * 4);
  // SDL_UnmapGPUTransferBuffer(Device, trBuf);
  //
  // SDL_GPUTextureTransferInfo tex_transfer_info{};
  // tex_transfer_info.transfer_buffer = trBuf;
  // tex_transfer_info.offset = 0;
  //
  // SDL_GPUTextureRegion tex_reg{};
  // {
  //   tex_reg.texture = textures_[0];
  //   tex_reg.w = (Uint32)img->w;
  //   tex_reg.h = (Uint32)img->h;
  //   tex_reg.d = 1;
  // }
  //
  // {
  //   SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(Device);
  //   SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuf);
  //   SDL_UploadToGPUTexture(copyPass, &tex_transfer_info, &tex_reg, false);
  //
  //   SDL_EndGPUCopyPass(copyPass);
  //   SDL_SubmitGPUCommandBuffer(cmdBuf);
  //   SDL_DestroySurface(img);
  //   SDL_ReleaseGPUTransferBuffer(Device, trBuf);
  // }

  return true;
}

bool
CubeProgram::SendVertexData()
{
  LOG_TRACE("CubeProgram::SendVertexData");
  auto& mesh = loader_.Meshes()[0];
  auto vert_count = mesh.vertices_.size();
  auto idx_count = mesh.indices_.size();
  LOG_DEBUG("Mesh has {} vertices and {} indices", vert_count, idx_count);

  SDL_GPUBufferCreateInfo vertInfo{};
  {
    vertInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    vertInfo.size = static_cast<Uint32>(sizeof(PosUvVertex) * vert_count);
  }

  SDL_GPUBufferCreateInfo idxInfo{};
  {
    idxInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    idxInfo.size = static_cast<Uint32>(sizeof(Uint16) * idx_count);
  }

  SDL_GPUTransferBufferCreateInfo transferInfo{};
  {
    Uint32 sz = sizeof(PosUvVertex) * vert_count + sizeof(Uint16) * idx_count;
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferInfo.size = sz;
  }

  vbuffer_ = SDL_CreateGPUBuffer(Device, &vertInfo);
  ibuffer_ = SDL_CreateGPUBuffer(Device, &idxInfo);
  SDL_GPUTransferBuffer* transferBuffer =
    SDL_CreateGPUTransferBuffer(Device, &transferInfo);

  if (!vbuffer_ || !ibuffer_ || !transferBuffer) {
    LOG_ERROR("couldn't create buffers");
    return false;
  }

  // Transfer Buffer to send vertex data to GPU
  PosUvVertex* transferData =
    (PosUvVertex*)SDL_MapGPUTransferBuffer(Device, transferBuffer, false);
  if (!transferData) {
    LOG_ERROR("couldn't get mapping for transfer buffer");
    return false;
  }

  for (u32 i = 0; i < vert_count; ++i) {
    transferData[i] = mesh.vertices_[i];
  }

  Uint16* indexData = (Uint16*)&transferData[vert_count];
  for (u32 i = 0; i < idx_count; ++i) {
    indexData[i] = mesh.indices_[i];
  }

  SDL_UnmapGPUTransferBuffer(Device, transferBuffer);

  // Upload the transfer data to the GPU resources
  SDL_GPUCommandBuffer* uploadCmdBuf = SDL_AcquireGPUCommandBuffer(Device);
  if (!uploadCmdBuf) {
    LOG_ERROR("couldn't acquire command buffer");
    return false;
  }
  SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCmdBuf);

  SDL_GPUTransferBufferLocation trLoc = { .transfer_buffer = transferBuffer,
                                          .offset = 0 };
  SDL_GPUBufferRegion reg = { .buffer = vbuffer_,
                              .offset = 0,
                              .size = static_cast<Uint32>(sizeof(PosUvVertex) *
                                                          vert_count) };
  SDL_UploadToGPUBuffer(copyPass, &trLoc, &reg, false);

  trLoc.offset = sizeof(PosUvVertex) * vert_count;
  reg.buffer = ibuffer_;
  reg.size = sizeof(Uint16) * idx_count;

  SDL_UploadToGPUBuffer(copyPass, &trLoc, &reg, false);

  SDL_EndGPUCopyPass(copyPass);
  if (!SDL_SubmitGPUCommandBuffer(uploadCmdBuf)) {
    LOG_ERROR("couldn't submit copy pass command buffer");
    return false;
  }
  SDL_ReleaseGPUTransferBuffer(Device, transferBuffer);

  return true;
}

bool
CubeProgram::CreateSceneRenderTargets()
{
  LOG_TRACE("CubeProgram::CreateSceneRenderTargets");
  auto info = SDL_GPUTextureCreateInfo{};
  {
    info.type = SDL_GPU_TEXTURETYPE_2D,
    info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
    info.width = static_cast<Uint32>(vp_width_),
    info.height = static_cast<Uint32>(vp_height_),
    info.layer_count_or_depth = 1, info.num_levels = 1,
    info.sample_count = SDL_GPU_SAMPLECOUNT_1,
    info.usage =
      SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
  }
  color_target_ = SDL_CreateGPUTexture(Device, &info);

  info.format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
  info.usage =
    SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
  depth_target_ = SDL_CreateGPUTexture(Device, &info);

  return depth_target_ != nullptr && color_target_ != nullptr;
}

bool
CubeProgram::InitGui()
{
  LOG_TRACE("CubeProgram::InitGui");
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

ImDrawData*
CubeProgram::DrawGui()
{
  // Init frame:
  {
    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
  }

  // GUI stuff
  {
    if (ImGui::Begin("Scene")) {
      ImGui::Text("Hello world");
      ImGui::Image((ImTextureID)(intptr_t)color_target_,
                   ImVec2((float)vp_width_, (float)vp_height_));
      ImGui::End();
    }

    ImGui::ShowMetricsWindow();

    if (ImGui::Begin("Settings")) {
      if (ImGui::TreeNode("Camera")) {
        if (ImGui::SliderFloat("X", (float*)&camera_.Position.x, -50.f, 50.f) ||
            ImGui::SliderFloat("Y", (float*)&camera_.Position.y, -50.f, 50.f) ||
            ImGui::SliderFloat("Z", (float*)&camera_.Position.z, -50.f, 50.f)) {
          camera_.Touched = true;
        }
        ImGui::TreePop();
      }
      if (ImGui::TreeNode("Spin Cube")) {
        for (auto& rot : rotations_) {
          ImGui::InputFloat(rot.name, &rot.speed);
        }
        ImGui::TreePop();
      }
      if (ImGui::TreeNode("Instancing")) {
        ImGui::InputFloat("Spread", &instance_cfg.spread);
        ImGui::InputInt("Dimensions", (int*)&instance_cfg.dimension);
        ImGui::TreePop();
      }
      ImGui::InputInt("Texture index", &tex_idx);
      ImGui::Checkbox("Wireframe", &wireframe_);
      ImGui::End();
    }
  }

  ImGui::Render();
  return ImGui::GetDrawData();
}
