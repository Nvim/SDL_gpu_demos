#include <pch.h>

#include "pbr_app.h"

#include "common/camera.h"
#include "common/logger.h"
#include "common/types.h"
#include "common/util.h"

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

  RELEASE_IF(vertex_, SDL_ReleaseGPUShader);
  RELEASE_IF(fragment_, SDL_ReleaseGPUShader);
  RELEASE_IF(depth_target_, SDL_ReleaseGPUTexture);
  RELEASE_IF(color_target_, SDL_ReleaseGPUTexture);
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
    scenes_.push_back(std::move(ret));
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
  if constexpr (sizeof(MaterialDataBinding) != 32) {
    LOG_CRITICAL("size is {}", sizeof(MaterialDataBinding));
    return false;
  }
  static const SDL_GPUViewport scene_vp{
    0, 0, float(vp_width_), float(vp_height_), 0.1f, 1.0f
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
  SceneDataBinding scene_data{ vp,
                               camera_.Model(),
                               glm::vec4{ camera_.Position, 0.f },
                               glm::vec4{ light_pos_, 0.f },
                               glm::vec4{ .9f, .9f, .9f, .1f },
                               instance_cfg.spread,
                               instance_cfg.dimension };

  ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, cmdbuf);

  stats_.Reset(); // Reset stats after GUI has drawn
  // Scene Pass
  {
    scene_color_target_info_.texture = color_target_;
    scene_depth_target_info_.texture = depth_target_;
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
  {
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    info.width = static_cast<Uint32>(vp_width_);
    info.height = static_cast<Uint32>(vp_height_);
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    info.sample_count = SDL_GPU_SAMPLECOUNT_1;
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
