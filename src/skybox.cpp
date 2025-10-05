#include "skybox.h"
#include "src/logger.h"
#include "util.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_assert.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_surface.h>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <vector>

Skybox::Skybox(const char* dir, SDL_Window* window, SDL_GPUDevice* device)
  : dir_{ dir }
  , device_{ device }
  , window_{ window }
{
  if (!Init()) {
    LOG_ERROR("Skybox initialization failed");
  }
}

Skybox::Skybox(const char* dir,
               const char* vert_path,
               const char* frag_path,
               SDL_Window* window,
               SDL_GPUDevice* device)
  : VertPath{ vert_path }
  , FragPath{ frag_path }
  , dir_{ dir }
  , device_{ device }
  , window_{ window }
{
  if (!Init()) {
    LOG_ERROR("Skybox initialization failed");
  }
}

Skybox::~Skybox()
{
  LOG_TRACE("Destroying Skybox");
  auto* Device = device_;
  for (const auto tx : faces) {
    RELEASE_IF(tx, SDL_ReleaseGPUTexture)
  }
  RELEASE_IF(Cubemap, SDL_ReleaseGPUTexture);
  RELEASE_IF(CubemapSampler, SDL_ReleaseGPUSampler);
  RELEASE_IF(Pipeline, SDL_ReleaseGPUGraphicsPipeline);
}

bool
Skybox::Init()
{
  LOG_TRACE("Skybox::Init");
  if (!CreatePipeline()) {
    LOG_ERROR("Couldn't create skybox pipeline");
    return false;
  }
  SDL_GPUBufferCreateInfo bufInfo{};
  {
    bufInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    bufInfo.size = sizeof(PosVertex) * 24;
  }
  VertexBuffer = SDL_CreateGPUBuffer(device_, &bufInfo);

  {
    bufInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    bufInfo.size = sizeof(Uint16) * 36;
  }
  IndexBuffer = SDL_CreateGPUBuffer(device_, &bufInfo);

  SDL_GPUSamplerCreateInfo samplerInfo{};
  {
    samplerInfo.min_filter = SDL_GPU_FILTER_NEAREST;
    samplerInfo.mag_filter = SDL_GPU_FILTER_NEAREST;
    samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    samplerInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  }
  CubemapSampler = SDL_CreateGPUSampler(device_, &samplerInfo);

  if (!SendVertexData()) {
    LOG_ERROR("couldn't send skybox vertex data");
    return false;
  }

  if (!LoadTextures()) {
    LOG_ERROR("couldn't load skybox textures");
    return false;
  }

  LOG_INFO("Initialized skybox");
  loaded_ = true;
  return loaded_;
}

bool
Skybox::CreatePipeline()
{
  LOG_TRACE("Skybox::CreatePipeline");
  auto vert = LoadShader(VertPath, device_, 0, 1, 0, 0);
  if (vert == nullptr) {
    LOG_ERROR("Couldn't load vertex shader at path {}", VertPath);
    return false;
  }
  auto frag = LoadShader(FragPath, device_, 1, 1, 0, 0);
  if (frag == nullptr) {
    LOG_ERROR("Couldn't load fragment shader at path {}", FragPath);
    return false;
  }

  SDL_GPUColorTargetDescription col_desc = {};
  col_desc.format = SDL_GetGPUSwapchainTextureFormat(device_, window_);
  if (col_desc.format == SDL_GPU_TEXTUREFORMAT_INVALID) {
    LOG_ERROR("no swapchain format");
    return false;
  }

  SDL_GPUVertexBufferDescription vert_desc{};
  {
    vert_desc.slot = 0;
    vert_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    vert_desc.instance_step_rate = 0;
    vert_desc.pitch = sizeof(PosVertex);
  }

  SDL_GPUVertexAttribute vert_attr{};
  {
    vert_attr.buffer_slot = 0;
    vert_attr.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vert_attr.location = 0;
    vert_attr.offset = 0;
  }

  SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo = {};
  {
    pipelineCreateInfo.vertex_shader = vert;
    pipelineCreateInfo.fragment_shader = frag;
    pipelineCreateInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    {
      auto& state = pipelineCreateInfo.vertex_input_state;
      state.vertex_buffer_descriptions = &vert_desc;
      state.num_vertex_buffers = 1;
      state.vertex_attributes = &vert_attr;
      state.num_vertex_attributes = 1;
    }
    {
      auto& state = pipelineCreateInfo.target_info;
      state.color_target_descriptions = &col_desc;
      state.num_color_targets = 1;
    }
  }
  Pipeline = SDL_CreateGPUGraphicsPipeline(device_, &pipelineCreateInfo);

  SDL_ReleaseGPUShader(device_, vert);
  SDL_ReleaseGPUShader(device_, frag);

  auto ret = Pipeline != nullptr;
  if (ret) {
    LOG_DEBUG("Created skybox pipeline");
  } else {
    LOG_ERROR("Couldn't create skybox pipeline: {}", GETERR);
  }
  return ret;
}

bool
Skybox::SendVertexData() const
{
  LOG_TRACE("Skybox::SendVertexData");
  Uint32 sz = (sizeof(PosVertex) * 24) + (sizeof(Uint16) * 36);
  SDL_GPUTransferBufferCreateInfo trInfo{};
  {
    trInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    trInfo.size = sz;
  }
  SDL_GPUTransferBuffer* trBuf = SDL_CreateGPUTransferBuffer(device_, &trInfo);

  { // Transfer buffer
    PosVertex* transferData =
      (PosVertex*)SDL_MapGPUTransferBuffer(device_, trBuf, false);
    Uint16* indexData = (Uint16*)&transferData[24];

    SDL_memcpy(transferData, verts_uvs, sizeof(verts_uvs));
    SDL_memcpy(indexData, indices, sizeof(indices));
    SDL_UnmapGPUTransferBuffer(device_, trBuf);
  }

  SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(device_);

  { // Copy pass
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdbuf);

    SDL_GPUTransferBufferLocation trLoc{ trBuf, 0 };
    SDL_GPUBufferRegion vBufReg{ VertexBuffer, 0, sizeof(PosVertex) * 24 };
    SDL_GPUBufferRegion iBufReg{ IndexBuffer, 0, sizeof(Uint16) * 36 };

    SDL_UploadToGPUBuffer(copyPass, &trLoc, &vBufReg, false);
    trLoc.offset = vBufReg.size;
    SDL_UploadToGPUBuffer(copyPass, &trLoc, &iBufReg, false);
    SDL_EndGPUCopyPass(copyPass);
  }

  SDL_ReleaseGPUTransferBuffer(device_, trBuf);
  auto ret = SDL_SubmitGPUCommandBuffer(cmdbuf);
  if (ret) {
    LOG_DEBUG("Sent skybox vertex data to GPU");
  } else {
    LOG_ERROR("Couldn't submit command buffer for vertex transfer: {}", GETERR);
  }
  return ret;
}

bool
Skybox::LoadTextures()
{
  LOG_TRACE("Skybox::LoadTextures");
  std::vector<SDL_Surface*> imgs;
  { // Load all faces in memory
    for (int i = 0; i < 6; ++i) {
      char pth[256];
      snprintf(pth, 256, "%s/%s", dir_, paths[i]);
      auto img = LoadImage(pth);
      if (!img) {
        LOG_ERROR("couldn't load skybox texture: {}", GETERR);
        for (int j = 0; j < i; ++j) {
          SDL_DestroySurface(imgs[j]);
        }
        return false;
      }
      imgs.push_back(img);
    }
  }
  auto freeImg = [](SDL_Surface* i) { SDL_DestroySurface(i); };
  Uint32 bytes_per_px =
    SDL_GetPixelFormatDetails(imgs[0]->format)->bytes_per_pixel;
  SDL_assert(bytes_per_px == 4);
  Uint32 imgW = imgs[0]->w;
  Uint32 imgH = imgs[0]->h;
  Uint32 imgSz = imgs[0]->h * imgs[0]->w * bytes_per_px;

  SDL_GPUTransferBufferCreateInfo info{};
  {
    info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    info.size = imgSz * 6;
  };
  SDL_GPUTransferBuffer* trBuf = SDL_CreateGPUTransferBuffer(device_, &info);

  if (!trBuf) {
    std::for_each(imgs.begin(), imgs.end(), freeImg);
    LOG_ERROR("couldn't create GPU transfer buffer: {}", GETERR);
    return false;
  }

  { // Map transfer buffer and memcpy data
    Uint8* textureTransferPtr =
      (Uint8*)SDL_MapGPUTransferBuffer(device_, trBuf, false);
    if (!textureTransferPtr) {
      std::for_each(imgs.begin(), imgs.end(), freeImg);
      SDL_ReleaseGPUTransferBuffer(device_, trBuf);
      LOG_ERROR("couldn't get transfer buffer mapping: {}", GETERR);
      return false;
    }
    for (int i = 0; i < 6; ++i) {
      const auto& img = imgs[i];
      SDL_memcpy(textureTransferPtr + (imgSz * i), img->pixels, imgSz);
      SDL_DestroySurface(img); // don't need this anymore
    }
    SDL_UnmapGPUTransferBuffer(device_, trBuf);
  }

  { // Create cubemap with right dimensions now that we got image size
    SDL_GPUTextureCreateInfo cubeMapInfo{};
    {
      cubeMapInfo.type = SDL_GPU_TEXTURETYPE_CUBE;
      cubeMapInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
      cubeMapInfo.width = imgW;
      cubeMapInfo.height = imgH;
      cubeMapInfo.layer_count_or_depth = 6;
      cubeMapInfo.num_levels = 1;
      cubeMapInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    };
    Cubemap = SDL_CreateGPUTexture(device_, &cubeMapInfo);
    if (!Cubemap) {
      LOG_ERROR("couldn't create cubemap texture: {}", GETERR);
      SDL_ReleaseGPUTransferBuffer(device_, trBuf);
      return false;
    }
  }

  { // Copy pass transfer buffer to GPU
    SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(device_);
    if (!cmdbuf) {
      LOG_ERROR("couldn't get command buffer for copy pass: {}", GETERR);
      SDL_ReleaseGPUTransferBuffer(device_, trBuf);
      return false;
    }

    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdbuf);

    for (Uint32 i = 0; i < 6; i += 1) {
      SDL_GPUTextureTransferInfo trInfo{};
      {
        trInfo.transfer_buffer = trBuf;
        trInfo.offset = imgSz * i;
      }
      SDL_GPUTextureRegion texReg{};
      {
        texReg.texture = Cubemap;
        texReg.layer = i;
        texReg.w = imgW;
        texReg.h = imgH;
        texReg.d = 1;
      };
      SDL_UploadToGPUTexture(copyPass, &trInfo, &texReg, false);
    }
    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(cmdbuf);
  }

  SDL_ReleaseGPUTransferBuffer(device_, trBuf);
  LOG_DEBUG("Loaded skybox textures");
  return true;
}

void
Skybox::Draw(SDL_GPURenderPass* pass) const
{
  static const SDL_GPUTextureSamplerBinding texBind{ Cubemap, CubemapSampler };
  static const SDL_GPUBufferBinding vBufBind{ VertexBuffer, 0 };
  static const SDL_GPUBufferBinding iBufBind{ IndexBuffer, 0 };

  SDL_BindGPUGraphicsPipeline(pass, Pipeline);
  SDL_BindGPUVertexBuffers(pass, 0, &vBufBind, 1);
  SDL_BindGPUIndexBuffer(pass, &iBufBind, SDL_GPU_INDEXELEMENTSIZE_16BIT);
  SDL_BindGPUFragmentSamplers(pass, 0, &texBind, 1);
  SDL_DrawGPUIndexedPrimitives(pass, 36, 1, 0, 0, 0);
}
