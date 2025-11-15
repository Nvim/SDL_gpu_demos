#include <pch.h>

#include "pbr_app.h"

#include "common/camera.h"
#include "common/cubemap.h"
#include "common/logger.h"
#include "common/types.h"
#include "common/util.h"
#include "shaders/debug_flags.h"

#include <glm/gtc/constants.hpp>
#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/backends/imgui_impl_sdlgpu3.h>
#include <imgui/imgui.h>

CubeProgram::CubeProgram(SDL_GPUDevice* device,
                         SDL_Window* window,
                         int w,
                         int h)
  : Program{ device, window }
  , vp_width_{ w }
  , vp_height_{ h }
{
  {
    scene_color_target_info_.clear_color = { 0.1f, 0.1f, 0.1f, 1.0f };
    scene_color_target_info_.load_op = SDL_GPU_LOADOP_CLEAR;
    scene_color_target_info_.store_op = SDL_GPU_STOREOP_STORE;
  }

  {
    post_process_target_info_.load_op = SDL_GPU_LOADOP_DONT_CARE;
    post_process_target_info_.store_op = SDL_GPU_STOREOP_STORE;
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
      &global_transform_.rotation_.x,
      0.f,
    };
    rotations_[1] = Rotation{
      "Y Axis",
      &global_transform_.rotation_.y,
      0.f,
    };
    rotations_[2] = Rotation{
      "Z Axis",
      &global_transform_.rotation_.z,
      0.f,
    };
  }
}

CubeProgram::~CubeProgram()
{
  LOG_TRACE("Destroying app");
  for (auto it = scenes_.begin(); it != scenes_.end(); it++) {
    it->get()->Release();
  }

  RELEASE_IF(depth_target_, SDL_ReleaseGPUTexture);
  RELEASE_IF(hdr_color_target_, SDL_ReleaseGPUTexture);
  RELEASE_IF(post_processed_target_, SDL_ReleaseGPUTexture);
  RELEASE_IF(brdf_lut_, SDL_ReleaseGPUTexture);
  RELEASE_IF(post_process_pipeline, SDL_ReleaseGPUComputePipeline);
  loader_.Release();

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

  if (!skybox_.IsLoaded()) {
    LOG_CRITICAL("Couldn't load skybox");
    return false;
  }

  scenes_.push_back(loader_.Load(default_scene_path_));
  if (!scenes_[0].get()) {
    LOG_CRITICAL("Couldn't initialize GLTF loader");
    return false;
  }
  assert(!scenes_[0]->Meshes().empty());
  LOG_INFO("Loaded {} meshes", scenes_[0]->Meshes().size());

  if (!CreateSceneRenderTargets()) {
    LOG_ERROR("Couldn't create render target textures!");
    return false;
  }
  LOG_DEBUG("Created render target textures");

  if (!LoadPbrTextures()) {
    LOG_CRITICAL("Couldn't load pbr textures");
    return false;
  }
  LOG_DEBUG("Loaded pbr textures");

  if (!CreatePostProcessPipeline()) {
    LOG_CRITICAL("Couldn't create post-process pipeline");
    return false;
  }
  LOG_DEBUG("Created post-process pipeline");

  global_transform_.translation_ = { 0.f, 0.f, 0.0f };
  global_transform_.scale_ = { 1.f, 1.f, 1.f };

  camera_.Position = glm::vec3{ 0.f, 0.f, 6.5f };

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
  // Poll input
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

  static auto last_asset = scenes_[0]->Path;
  if (scene_picker_.CurrentAsset != last_asset && !is_loading_scene) {
    ChangeScene();
  }

  // Poll future:
  if (scene_future_.valid() && scene_future_.wait_for(std::chrono::nanoseconds(
                                 0)) == std::future_status::ready) {
    is_loading_scene = false;
    auto ret = scene_future_.get();
    if (ret == nullptr) {
      LOG_ERROR("Failed loading scene `{}`",
                scene_picker_.CurrentAsset.c_str());
      scene_picker_.CurrentAsset = last_asset; // restore former scene
      return true;
    }
    last_asset = scene_picker_.CurrentAsset;
    // scenes_.push_back(std::move(ret));
    scenes_[0] = std::move(ret);
  }

  return true;
}

void
CubeProgram::UpdateScene()
{
  for (const auto& rot : rotations_) {
    if (rot.speed != 0.f) {
      *rot.axis =
        glm::mod(*rot.axis + DeltaTime * rot.speed, glm::two_pi<float>());
      global_transform_.Touched = true;
    }
  }

  for (const auto& scene : scenes_) {
    for (const auto& node : scene->ParentNodes()) {
      node->Update(global_transform_.Matrix());
    }
  }
  camera_.Update(DeltaTime);
}

bool
CubeProgram::Draw()
{
  static const SDL_GPUViewport scene_vp{
    0, 0, float(vp_width_), float(vp_height_), 0.1f, 1.0f
  };

  static const SDL_GPUTextureSamplerBinding pbr_sampler_binds[3]{
    { brdf_lut_, pbr_samplers_[0] },
    { irradiance_map_->Texture, pbr_samplers_[1] },
    { specular_map_->Texture, pbr_samplers_[2] }
  };

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
  auto vp = camera_.Projection() * camera_.View();
  auto draw_data = DrawGui();
  auto d = instance_cfg.dimension;
  auto total_instances = d * d * d;
  SceneDataBinding scene_data{
    vp,
    camera_.Model(),
    glm::vec4{ camera_.Position, 0.f },
    glm::vec4{ light_pos_, 0.f },
    glm::vec4{ .9f, .9f, .9f, .1f },
    instance_cfg.spread,
    instance_cfg.dimension,
    shader_debug_flags_,
  };

  ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, cmdbuf);

  stats_.Reset(); // Reset stats after GUI has drawn
  // Scene Pass
  {
    SDL_PushGPUVertexUniformData(cmdbuf, 0, &scene_data, sizeof(scene_data));
    SDL_PushGPUFragmentUniformData(cmdbuf, 0, &scene_data, sizeof(scene_data));

    SDL_GPURenderPass* scenePass = SDL_BeginGPURenderPass(
      cmdbuf, &scene_color_target_info_, 1, &scene_depth_target_info_);

    SDL_SetGPUViewport(scenePass, &scene_vp);

    auto DrawCall = [&](const RenderItem& draw) {
      assert(draw.VertexBuffer != nullptr);
      assert(draw.IndexBuffer != nullptr);
      const SDL_GPUBufferBinding vBinding{ draw.VertexBuffer, 0 };
      const SDL_GPUBufferBinding iBinding{ draw.IndexBuffer, 0 };

      // Geometry
      SDL_BindGPUVertexBuffers(scenePass, 0, &vBinding, 1);
      SDL_BindGPUIndexBuffer(
        scenePass, &iBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
      DrawDataBinding b{ draw.matrix };
      SDL_PushGPUVertexUniformData(cmdbuf, 1, &b, sizeof(b));

      // Material
      auto material = draw.Material;
      SDL_BindGPUGraphicsPipeline(scenePass, material->Pipeline);

      material->ubo.Bind(cmdbuf);

      material->BindSamplers(scenePass);
      SDL_BindGPUFragmentSamplers(
        scenePass, material->TextureCount, pbr_sampler_binds, 3);
      SDL_DrawGPUIndexedPrimitives(
        scenePass, draw.VertexCount, total_instances, draw.FirstIndex, 0, 0);
    };

    for (const auto& scene : scenes_) {
      scene->Draw(glm::mat4{ 1.0f }, render_context_);
    }
    for (const auto& draw : render_context_.OpaqueItems) {
      DrawCall(draw);
      stats_.opaque_draws++;
      stats_.total_draws++;
    }
    for (const auto& draw : render_context_.TransparentItems) {
      DrawCall(draw);
      stats_.transparent_draws++;
      stats_.total_draws++;
    }
    if (skybox_toggle_) {
      skybox_.Draw(scenePass);
    }

    render_context_.Clear();
    SDL_EndGPURenderPass(scenePass);
  }

  // Post-Process pass
  {
    SDL_GPUStorageTextureReadWriteBinding tex_bind{};
    {
      tex_bind.texture = post_processed_target_;
      tex_bind.cycle = true;
    }
    SDL_GPUComputePass* pass =
      SDL_BeginGPUComputePass(cmdbuf, &tex_bind, 1, nullptr, 0);

    SDL_BindGPUComputePipeline(pass, post_process_pipeline);
    SDL_BindGPUComputeStorageTextures(pass, 0, &hdr_color_target_, 1);
    SDL_DispatchGPUCompute(pass, vp_width_ / 8, vp_height_ / 8, 1);

    SDL_EndGPUComputePass(pass);
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

void
CubeProgram::ChangeScene()
{
  if (is_loading_scene) {
    LOG_WARN("a scene is already loading, ignoring request");
    return;
  }
  is_loading_scene = true;
  LOG_INFO("loading scene {}", scene_picker_.CurrentAsset.c_str());
  scene_future_ = std::async([this]() {
    // std::this_thread::sleep_for(std::chrono::seconds(3));
    return loader_.Load(scene_picker_.CurrentAsset);
    // return ret;
    // return std::unique_ptr<GLTFScene>(nullptr);
  });
}

bool
CubeProgram::CreateSceneRenderTargets()
{
  LOG_TRACE("CubeProgram::CreateSceneRenderTargets");
  auto info = SDL_GPUTextureCreateInfo{};

  { // HDR scene pass target
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = HDR_TARGET_FORMAT;
    info.width = static_cast<Uint32>(vp_width_);
    info.height = static_cast<Uint32>(vp_height_);
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER |
                 SDL_GPU_TEXTUREUSAGE_COLOR_TARGET |
                 SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ;
    hdr_color_target_ = SDL_CreateGPUTexture(Device, &info);
  }

  { // Post processing pass target (HDR -> SDR compute shader)
    info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER |
                 SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ |
                 SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE;
    post_processed_target_ = SDL_CreateGPUTexture(Device, &info);
  }

  { // Depth target
    info.format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
    info.usage =
      SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    depth_target_ = SDL_CreateGPUTexture(Device, &info);
  }

  post_process_target_info_.texture = post_processed_target_;
  scene_color_target_info_.texture = hdr_color_target_;
  scene_depth_target_info_.texture = depth_target_;

  return depth_target_ != nullptr && hdr_color_target_ != nullptr &&
         post_processed_target_ != nullptr;
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

struct PbrFlag
{
  const char* label;
  u32 flag_value;
};

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
      ImGui::Image((ImTextureID)(intptr_t)post_processed_target_,
                   ImVec2((float)vp_width_, (float)vp_height_));
      ImGui::End();
    }
    if (ImGui::Begin("Stats")) {
      ImGui::Text("Total draws: %u", stats_.total_draws);
      ImGui::Text("Opaque draws: %u", stats_.opaque_draws);
      ImGui::Text("Transparent draws: %u", stats_.transparent_draws);
      ImGui::End();
    }

    ImGui::ShowMetricsWindow();

    if (ImGui::Begin("Settings")) {
      if (ImGui::TreeNode("Camera")) {
        ImGui::Text("Position");
        if (ImGui::SliderFloat("X", (float*)&camera_.Position.x, -50.f, 50.f) ||
            ImGui::SliderFloat("Y", (float*)&camera_.Position.y, -50.f, 50.f) ||
            ImGui::SliderFloat("Z", (float*)&camera_.Position.z, -50.f, 50.f)) {
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
        if (ImGui::SliderFloat3("Position", (float*)&light_pos_, -20.f, 20.f)) {
        }
        ImGui::TreePop();
      }
      if (ImGui::TreeNode("PBR Settings")) {
        // Each flag's name and the corresponding bit
        static constexpr PbrFlag flags[] = {
          { "USE_DIFFUSE_TEX", USE_DIFFUSE_TEX },
          { "USE_VERTEX_COLOR", USE_VERTEX_COLOR },
          { "USE_NORMAL_TEX", USE_NORMAL_TEX },
          { "USE_NORMAL_FACT", USE_NORMAL_FACT },
          { "USE_METAL_TEX", USE_METAL_TEX },
          { "USE_ROUGH_TEX", USE_ROUGH_TEX },
          { "USE_OCCLUSION_TEX", USE_OCCLUSION_TEX },
          { "USE_OCCLUSION_FACT", USE_OCCLUSION_FACT },
          { "USE_EMISSIVE_FACT", USE_EMISSIVE_FACT },
          { "USE_EMISSIVE_TEX", USE_EMISSIVE_TEX },
          { "USE_IBL_SPECULAR", USE_IBL_SPECULAR },
          { "USE_IBL_DIFFUSE", USE_IBL_DIFFUSE },
          { "USE_POINTLIGHTS", USE_POINTLIGHTS },
          { "USE_TONEMAPPING", USE_TONEMAPING },
          { "USE_GAMMA_CORRECT", USE_GAMMA_CORRECT },
        };

        if (ImGui::Button("Disable all")) {
          shader_debug_flags_ = 0;
        }
        if (ImGui::Button("Enable all")) {
          shader_debug_flags_ = 0xFFFFFFFF;
        }

        for (const PbrFlag& flag : flags) {
          bool flag_set = (shader_debug_flags_ & flag.flag_value) != 0;
          if (ImGui::Checkbox(flag.label, &flag_set)) {
            if (flag_set) {
              shader_debug_flags_ |= flag.flag_value;
            } else {
              shader_debug_flags_ &= ~flag.flag_value;
            }
          }
        }
        ImGui::TreePop();
      }
      if (ImGui::TreeNode("Spin")) {
        for (auto& rot : rotations_) {
          ImGui::InputFloat(rot.name, &rot.speed);
        }
        ImGui::TreePop();
      }
      f32 scale = global_transform_.scale_.x;
      if (ImGui::TreeNode("Scale")) {
        if (ImGui::InputFloat("All axis", &scale)) {
          global_transform_.scale_ = glm::vec3{ scale };
          global_transform_.Touched = true;
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
      ImGui::Checkbox("Skybox", &skybox_toggle_);
      ImGui::End();
    }

    scene_picker_.Render(is_loading_scene);
  }

  ImGui::Render();
  return ImGui::GetDrawData();
}

bool
CubeProgram::LoadPbrTextures()
{
  KtxCubemapLoader loader(Device);
  irradiance_map_ =
    loader.Load(IRRADIANCE_MAP_PATH, CubeMapUsage::IrradianceMap);
  if (irradiance_map_ == nullptr) {
    LOG_ERROR("Couldn't load irradiance map");
    return false;
  }
  specular_map_ = loader.Load(SPECULAR_MAP_PATH, CubeMapUsage::SpecularMap);
  if (specular_map_ == nullptr) {
    LOG_ERROR("Couldn't load specular map");
    return false;
  }

  {
    LoadedImage img{};
    constexpr auto tex_format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
    { // Load texture data to cpu and create texture
      auto type = ImageType::DIMENSIONS_2D;
      auto f = ImagePixelFormat::PIXELFORMAT_UINT;
      if (!ImageLoader::Load(img, BRDF_LUT_PATH, type, f)) {
        LOG_ERROR("couldn't load brdf lut image");
        return false;
      }
      SDL_GPUTextureCreateInfo tex_info{};
      {
        tex_info.type = SDL_GPU_TEXTURETYPE_2D;
        tex_info.format = tex_format;
        tex_info.height = img.h;
        tex_info.width = img.w;
        tex_info.layer_count_or_depth = 1;
        tex_info.num_levels = 1;
        tex_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
        tex_info.sample_count = SDL_GPU_SAMPLECOUNT_1;
        // tex_info.props = ;
      }
      brdf_lut_ = SDL_CreateGPUTexture(Device, &tex_info);
      if (!brdf_lut_) {
        LOG_ERROR("Couldn't create texture: {}", GETERR);
        return false;
      }
    }

    SDL_GPUSampler* sampler{ nullptr };
    {
      SDL_GPUSamplerCreateInfo samplerInfo{};
      {
        samplerInfo.min_filter = SDL_GPU_FILTER_NEAREST;
        samplerInfo.mag_filter = SDL_GPU_FILTER_NEAREST;
        samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
      }
      sampler = SDL_CreateGPUSampler(Device, &samplerInfo);
      if (!sampler) {
        LOG_ERROR("Couldn't create brdf LUT sampler, {}", GETERR);
        return false;
      }
      for (u8 i = 0; i < 3; ++i) {
        pbr_samplers_[i] = sampler;
      }
    }

    SDL_GPUTransferBuffer* trBuf;
    { // Memcpy data from cpu -> transfer buffer
      SDL_GPUTransferBufferCreateInfo info{};
      {
        info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        info.size = img.DataSize();
      };
      trBuf = SDL_CreateGPUTransferBuffer(Device, &info);
      if (!trBuf) {
        SDL_ReleaseGPUTexture(Device, brdf_lut_);
        LOG_ERROR("couldn't create GPU transfer buffer: {}", GETERR);
        return false;
      }
      u8* mapped = (u8*)SDL_MapGPUTransferBuffer(Device, trBuf, false);
      if (!mapped) {
        SDL_ReleaseGPUTexture(Device, brdf_lut_);
        SDL_ReleaseGPUTransferBuffer(Device, trBuf);
        LOG_ERROR("couldn't get transfer buffer mapping: {}", GETERR);
        return false;
      }
      SDL_memcpy(mapped, img.data, img.DataSize());
      SDL_UnmapGPUTransferBuffer(Device, trBuf);
    }

    SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(Device);
    if (cmdbuf == NULL) {
      LOG_ERROR("Couldn't acquire command buffer: {}", GETERR);
      return false;
    }

    { // Copy pass transfer buffer -> GPU texture
      SDL_GPUTextureTransferInfo tex_transfer_info{};
      {
        tex_transfer_info.transfer_buffer = trBuf;
        tex_transfer_info.offset = 0;
      }
      SDL_GPUTextureRegion tex_reg{};
      {
        tex_reg.texture = brdf_lut_;
        tex_reg.w = img.w;
        tex_reg.h = img.h;
        tex_reg.d = 1;
      }
      SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdbuf);
      SDL_UploadToGPUTexture(copyPass, &tex_transfer_info, &tex_reg, false);

      SDL_EndGPUCopyPass(copyPass);
    }
    SDL_ReleaseGPUTransferBuffer(Device, trBuf);

    if (!SDL_SubmitGPUCommandBuffer(cmdbuf)) {
      LOG_ERROR("couldn't submit command buffer: {}", GETERR);
      return false;
    }
  }
  return true;
}

bool
CubeProgram::CreatePostProcessPipeline()
{
  LOG_TRACE("CubeProgram::CreatePostProcessPipeline");

  u64 codeSize;
  void* code = SDL_LoadFile(POST_PROCESS_PATH, &codeSize);
  if (code == nullptr) {
    LOG_ERROR("Couldn't load compute shader file: {}", GETERR);
    return false;
  }

  SDL_GPUColorTargetDescription color_descs[1]{};
  {
    auto& d = color_descs[0];
    d.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  }

  SDL_GPUComputePipelineCreateInfo pipelineInfo{};
  {
    pipelineInfo.code = (u8*)code;
    pipelineInfo.code_size = codeSize;
    pipelineInfo.entrypoint = "main";
    pipelineInfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
    pipelineInfo.num_samplers = 0;
    pipelineInfo.num_readonly_storage_textures = 1;
    pipelineInfo.num_readonly_storage_buffers = 0;
    pipelineInfo.num_readwrite_storage_textures = 1;
    pipelineInfo.num_readwrite_storage_buffers = 0;
    pipelineInfo.num_uniform_buffers = 0;
    pipelineInfo.threadcount_x = 16;
    pipelineInfo.threadcount_y = 16;
    pipelineInfo.threadcount_z = 1;
  }

  post_process_pipeline = SDL_CreateGPUComputePipeline(Device, &pipelineInfo);
  SDL_free(code);

  if (post_process_pipeline == nullptr) {
    LOG_ERROR("Couldn't create compute pipeline: {}", GETERR);
    return false;
  }
  return true;
}
