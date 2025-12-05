#include "engine.h"

#include "common/cubemap.h"
#include "common/gltf_loader.h"
#include "common/logger.h"
#include "common/types.h"
#include <SDL3/SDL_gpu.h>

Engine::Engine(SDL_GPUDevice* device, SDL_Window* window)
  : Device{ device }
  , MultifileCubemapLoader{ device }
  , KtxCubemapLoader{ device }
  , ProjectionCubemapLoader{ device }
  , window_{ window }
{
}

Engine::~Engine()
{
  RELEASE_IF(default_texture_, SDL_ReleaseGPUTexture)
  RELEASE_IF(linear_clamp_sampler_, SDL_ReleaseGPUSampler);
  RELEASE_IF(linear_repeat_sampler_, SDL_ReleaseGPUSampler);
}

bool
Engine::Init()
{
  LOG_TRACE("Engine::Init");
  LOG_INFO("Initializing Engine");

  if (!CreateDefaultTexture()) {
    LOG_ERROR("Couldn't create default texture");
    return false;
  }
  if (!CreateDefaultSamplers()) {
    LOG_ERROR("Couldn't create default samplers");
    return false;
  }
  return true;
}

bool
Engine::UploadTo2DTexture(SDL_GPUTexture* tex, LoadedImage& img)
{
  LOG_TRACE("Engine::UploadTo2DTexture");
  if (tex == nullptr) {
    LOG_ERROR("Couldn't upload to texture: invalid texture");
    return false;
  }

  TransferBufferWrapper tr_wrapped{ Device, static_cast<u32>(img.DataSize()) };
  auto* tr_buf = tr_wrapped.Get();
  if (!tr_buf) {
    LOG_ERROR("Couldn't create transfer buffer: {}", SDL_GetError);
    return false;
  }

  { // Setup transfer buffer
    void* memory = SDL_MapGPUTransferBuffer(Device, tr_buf, false);
    if (!memory) {
      LOG_ERROR("Couldn't map texture memory: {}", SDL_GetError());
      return false;
    }
    SDL_memcpy(memory, img.data, img.DataSize());
    SDL_UnmapGPUTransferBuffer(Device, tr_buf);
  }

  { // Copy pass
    SDL_GPUTextureTransferInfo tex_transfer_info{};
    tex_transfer_info.transfer_buffer = tr_buf;
    tex_transfer_info.offset = 0;

    SDL_GPUTextureRegion tex_reg{};
    {
      tex_reg.texture = tex;
      tex_reg.w = (Uint32)img.w;
      tex_reg.h = (Uint32)img.h;
      tex_reg.d = 1;
    }
    SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(Device);
    if (!cmdBuf) {
      LOG_ERROR("Couldn't acquire command buffer: {}", SDL_GetError());
      return false;
    }
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuf);
    SDL_UploadToGPUTexture(copyPass, &tex_transfer_info, &tex_reg, false);

    SDL_EndGPUCopyPass(copyPass);
    if (!SDL_SubmitGPUCommandBuffer(cmdBuf)) {
      LOG_ERROR("Couldn't sumbit command buffer: {}", SDL_GetError());
    }
  }
  return true;
}

bool
Engine::CreateDefaultSamplers()
{
  LOG_TRACE("Engine::CreateDefaultSamplers");
  SDL_GPUSamplerCreateInfo info{};
  {
    info.min_filter = SDL_GPU_FILTER_LINEAR;
    info.mag_filter = SDL_GPU_FILTER_LINEAR;
    info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
    info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
  }
  linear_repeat_sampler_ = SDL_CreateGPUSampler(Device, &info);
  if (!linear_repeat_sampler_) {
    LOG_ERROR("Couldn't create sampler: {}", GETERR);
    return false;
  }
  {
    info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  }
  linear_clamp_sampler_ = SDL_CreateGPUSampler(Device, &info);
  if (!linear_clamp_sampler_) {
    LOG_ERROR("Couldn't create sampler: {}", GETERR);
    return false;
  }
  return true;
}

bool
Engine::CreateDefaultTexture()
{
  LOG_TRACE("Engine::CreateDefaultTexture");

  { // Create texture
    SDL_GPUTextureCreateInfo tex_info{};
    {
      tex_info.type = SDL_GPU_TEXTURETYPE_2D;
      tex_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
      tex_info.width = 32;
      tex_info.height = 32;
      tex_info.layer_count_or_depth = 1;
      tex_info.num_levels = 1;
      tex_info.usage =
        SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    }
    default_texture_ = SDL_CreateGPUTexture(Device, &tex_info);

    if (!default_texture_) {
      LOG_ERROR("Default texture creation failed: {}", SDL_GetError());
      return false;
    }
  }

  { // Clear texture
    SDL_GPUColorTargetInfo colorInfo{};
    {
      colorInfo.texture = default_texture_;
      colorInfo.clear_color = { 1.f, 1.f, 1.f, 1.f };
      colorInfo.load_op = SDL_GPU_LOADOP_CLEAR;
      colorInfo.store_op = SDL_GPU_STOREOP_STORE;
    };
    auto* cmdbuf = SDL_AcquireGPUCommandBuffer(Device);
    if (!cmdbuf) {
      LOG_ERROR("Couldn't acquire command buffer: {}", SDL_GetError());
      return false;
    }
    SDL_GPURenderPass* renderPass =
      SDL_BeginGPURenderPass(cmdbuf, &colorInfo, 1, NULL);
    SDL_EndGPURenderPass(renderPass);

    if (!SDL_SubmitGPUCommandBuffer(cmdbuf)) {
      LOG_ERROR("Couldn't submit command buffer: {}", SDL_GetError());
      return false;
    }
  }

  return true;
}
