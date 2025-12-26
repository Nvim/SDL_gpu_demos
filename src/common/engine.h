#pragma once

#include "common/cubemap.h"
#include "common/rendersystem.h"
#include "common/types.h"
#include <SDL3/SDL_gpu.h>

class GLTFLoader;
class GLTFScene;
struct LoadedImage;

// Aggregates common boilerplate GPU functions
class Engine
{
public:
  DISABLE_COPY_AND_MOVE(Engine);
  explicit Engine(SDL_GPUDevice* device, SDL_Window* window);
  ~Engine();

  bool Init();
  SDL_GPUTexture* DefaultTexture() const { return default_texture_; }
  SDL_GPUSampler* LinearRepeatSampler() const { return linear_repeat_sampler_; }
  SDL_GPUSampler* LinearClampSampler() const { return linear_clamp_sampler_; }

  // GPU Uploads:
  template<typename T>
  bool UploadToBuffer(SDL_GPUBuffer* buf, const T* data, const u32 size);
  template<typename V, typename I>
  bool CreateAndUploadMeshBuffers(MeshBuffers* buffers,
                                  const V* vertices,
                                  u32 vert_count,
                                  const I* indices,
                                  u32 idx_count);

  bool UploadTo2DTexture(SDL_GPUTexture* tex, LoadedImage& img);

private:
  bool CreateDefaultSamplers();
  bool CreateDefaultTexture();

public:
  SDL_GPUDevice* Device;
  MultifileCubemapLoader MultifileCubemapLoader;
  KtxCubemapLoader KtxCubemapLoader;
  ProjectionCubemapLoader ProjectionCubemapLoader;

private:
  SDL_Window* window_;
  SDL_GPUTexture* default_texture_{ nullptr };
  SDL_GPUSampler* linear_repeat_sampler_{ nullptr };
  SDL_GPUSampler* linear_clamp_sampler_{ nullptr };
};

template<typename T>
bool
Engine::UploadToBuffer(SDL_GPUBuffer* buf, const T* data, const u32 count)
{
  LOG_TRACE("Engine::UploadBuffers");
  if (buf == nullptr) {
    LOG_ERROR("Couldn't upload to buffer: invalid buffer");
    return false;
  }

  const u32 transfer_size = sizeof(T)*count;

  TransferBufferWrapper tr_wrapped{ Device, transfer_size };
  auto* tr_buf = tr_wrapped.Get();
  if (!tr_buf) {
    LOG_ERROR("Couldn't create transfer buffer: {}", SDL_GetError);
    return false;
  }

  { // setup transfer buffer
    T* transferData = (T*)SDL_MapGPUTransferBuffer(Device, tr_buf, false);
    if (!transferData) {
      LOG_ERROR("couldn't get mapping for transfer buffer");
      return false;
    }

    for (u32 i = 0; i < count; ++i) {
      transferData[i] = data[i];
    }
    SDL_UnmapGPUTransferBuffer(Device, tr_buf);
  }

  auto ret{ false };
  { // copy pass
    SDL_GPUCommandBuffer* uploadCmdBuf = SDL_AcquireGPUCommandBuffer(Device);
    if (!uploadCmdBuf) {
      LOG_ERROR("couldn't acquire command buffer");
      return false;
    }
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCmdBuf);

    SDL_GPUTransferBufferLocation trLoc;
    {
      trLoc.transfer_buffer = tr_buf;
      trLoc.offset = 0;
    }
    SDL_GPUBufferRegion reg;
    {
      reg.buffer = buf;
      reg.offset = 0;
      reg.size = transfer_size;
    };
    SDL_UploadToGPUBuffer(copyPass, &trLoc, &reg, false);

    SDL_EndGPUCopyPass(copyPass);
    ret = SDL_SubmitGPUCommandBuffer(uploadCmdBuf);
  }

  return ret;
}

template<typename V, typename I>
bool
Engine::CreateAndUploadMeshBuffers(MeshBuffers* buffers,
                                   const V* vertices,
                                   u32 vert_count,
                                   const I* indices,
                                   u32 idx_count)
{
  LOG_TRACE("Engine::CreateAndUploadMeshBuffers");
  u32 transfer_size = sizeof(V) * vert_count + sizeof(I) * idx_count;
  LOG_DEBUG("Mesh has {} vertices and {} indices", vert_count, idx_count);

  SDL_GPUBufferCreateInfo vertInfo{};
  {
    vertInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    vertInfo.size = static_cast<u32>(sizeof(V) * vert_count);
  }

  SDL_GPUBufferCreateInfo idxInfo{};
  {
    idxInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    idxInfo.size = static_cast<u32>(sizeof(I) * idx_count);
  }

  buffers->VertexBuffer = SDL_CreateGPUBuffer(Device, &vertInfo);
  auto vbuf = buffers->VertexBuffer;
  if (!vbuf) {
    LOG_ERROR("couldn't create vertex buffer");
    return false;
  }

  buffers->IndexBuffer = SDL_CreateGPUBuffer(Device, &idxInfo);
  auto ibuf = buffers->IndexBuffer;
  if (!ibuf) {
    LOG_ERROR("couldn't create index buffer");
    SDL_ReleaseGPUBuffer(Device, vbuf);
    return false;
  }

#define RELEASE_BUFFERS                                                        \
  SDL_ReleaseGPUBuffer(Device, vbuf);                                          \
  SDL_ReleaseGPUBuffer(Device, ibuf);

  TransferBufferWrapper tr_wrapped{ Device, transfer_size };
  auto* trBuf = tr_wrapped.Get();
  if (!trBuf) {
    LOG_ERROR("Couldn't create transfer buffer: {}", SDL_GetError);
    RELEASE_BUFFERS
    return false;
  }

  // Transfer Buffer to send vertex data to GPU
  V* transferData = (V*)SDL_MapGPUTransferBuffer(Device, trBuf, false);
  if (!transferData) {
    LOG_ERROR("couldn't get mapping for transfer buffer");
    RELEASE_BUFFERS
    return false;
  }

  for (u32 i = 0; i < vert_count; ++i) {
    transferData[i] = vertices[i];
  }

  I* indexData = (I*)&transferData[vert_count];
  for (u32 i = 0; i < idx_count; ++i) {
    indexData[i] = indices[i];
  }

  SDL_UnmapGPUTransferBuffer(Device, trBuf);

  // Upload the transfer data to the GPU resources
  SDL_GPUCommandBuffer* uploadCmdBuf = SDL_AcquireGPUCommandBuffer(Device);
  if (!uploadCmdBuf) {
    LOG_ERROR("couldn't acquire command buffer");
    RELEASE_BUFFERS
    return false;
  }
  SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCmdBuf);

  SDL_GPUTransferBufferLocation trLoc;
  {
    trLoc.transfer_buffer = trBuf;
    trLoc.offset = 0;
  }
  SDL_GPUBufferRegion reg;
  {
    reg.buffer = vbuf;
    reg.offset = 0;
    reg.size = static_cast<u32>(sizeof(V) * vert_count);
  };
  SDL_UploadToGPUBuffer(copyPass, &trLoc, &reg, false);

  trLoc.offset = sizeof(V) * vert_count;
  reg.buffer = ibuf;
  reg.size = sizeof(I) * idx_count;

  SDL_UploadToGPUBuffer(copyPass, &trLoc, &reg, false);

  SDL_EndGPUCopyPass(copyPass);
  bool ret = SDL_SubmitGPUCommandBuffer(uploadCmdBuf);
  if (!ret) {
    LOG_ERROR("couldn't submit copy pass command buffer");
    RELEASE_BUFFERS
  } else {
    LOG_DEBUG("Uploaded vertex data to GPU");
  }
  return ret;
#undef RELEASE_BUFFERS
}
