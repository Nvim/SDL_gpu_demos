#include <pch.h>

#include "common/cubemap.h"
#include "skybox.h"

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
  RELEASE_IF(Pipeline, SDL_ReleaseGPUGraphicsPipeline);
}

bool
Skybox::Init()
{
  LOG_TRACE("Skybox::Init");

  static MultifileCubeMapLoader loader{ device_ };
  auto format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM; // TODO: handle hdr?
  Cubemap = loader.Load(dir_, CubeMapUsage::Skybox, format);
  if (Cubemap == nullptr) {
    LOG_ERROR("couldn't load skybox textures");
    return false;
  }

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
      state.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
      state.has_depth_stencil_target = true;
    }
    {
      auto& state = pipelineCreateInfo.depth_stencil_state;
      state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
      state.enable_depth_test = true;
      state.enable_depth_write = false;
      state.enable_stencil_test = false;
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

void
Skybox::Draw(SDL_GPURenderPass* pass) const
{
  static const SDL_GPUTextureSamplerBinding texBind{ Cubemap->Texture,
                                                     CubemapSampler };
  static const SDL_GPUBufferBinding vBufBind{ VertexBuffer, 0 };
  static const SDL_GPUBufferBinding iBufBind{ IndexBuffer, 0 };

  SDL_BindGPUGraphicsPipeline(pass, Pipeline);
  SDL_BindGPUVertexBuffers(pass, 0, &vBufBind, 1);
  SDL_BindGPUIndexBuffer(pass, &iBufBind, SDL_GPU_INDEXELEMENTSIZE_16BIT);
  SDL_BindGPUFragmentSamplers(pass, 0, &texBind, 1);
  SDL_DrawGPUIndexedPrimitives(pass, 36, 1, 0, 0, 0);
}
