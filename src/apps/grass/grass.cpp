#include <pch.h>

#include "common/logger.h"
#include "grass.h"

#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/backends/imgui_impl_sdlgpu3.h>
#include <imgui/imgui.h>

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
  {
    scene_color_target_info_.clear_color = { .2f, 1.f, .7f, 1.f };
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
  LOG_DEBUG("Destroyed GrassProgram");
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
  if (!CreateRenderTargets()) {
    LOG_CRITICAL("Couldn't create render targets");
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
    SDL_GPURenderPass* scene_pass =
      SDL_BeginGPURenderPass(cmdbuf, &scene_color_target_info_, 1, nullptr);

    SDL_SetGPUViewport(scene_pass, &scene_vp);

    SDL_EndGPURenderPass(scene_pass);
  }

  { // Fullscreen pass
    swapchain_target_info_.texture = swapchain_tex;
    SDL_GPURenderPass* screen_pass =
      SDL_BeginGPURenderPass(cmdbuf, &swapchain_target_info_, 1, nullptr);

    ImGui_ImplSDLGPU3_RenderDrawData(draw_data, cmdbuf, screen_pass);
    SDL_EndGPURenderPass(screen_pass);
  }

  if (!SDL_SubmitGPUCommandBuffer(cmdbuf)) {
    LOG_ERROR("Couldn't submit command buffer: {}", GETERR)
    return false;
  }
  return true;
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
    info.format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
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

  if (ImGui::Begin("Settings")) {
    ImGui::Text("Window Width: %d", window_w_);
    ImGui::Text("Window Height: %d", window_h_);
    ImGui::Text("Rendertarget Width: %d", rendertarget_w_);
    ImGui::Text("Rendertarget Height: %d", rendertarget_h_);
    ImGui::End();
  }
  if (ImGui::Begin("aaa")) {
    ImGui::Text("Hello world");
    ImGui::Image((ImTextureID)(intptr_t)scene_target_,
                 ImVec2((float)rendertarget_w_, (float)rendertarget_h_));
    ImGui::End();
  }

  ImGui::Render();
  return ImGui::GetDrawData();
}
