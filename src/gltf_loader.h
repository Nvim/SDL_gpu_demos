#pragma once

#include "fastgltf/types.hpp"
#include "src/logger.h"
#include "src/material.h"
#include "src/program.h"
#include "types.h"
#include <SDL3/SDL_gpu.h>
#include <fastgltf/core.hpp>
#include <vector>

struct Geometry
{
  const std::size_t FirstIndex;
  const std::size_t VertexCount;
  std::shared_ptr<PbrMaterial> material{ nullptr };
};
struct LoadedImage
{
  int w, h,
    nrChannels; // NOTE: don't use! Channel count is always forced to 4
  u8* data{ nullptr };
};

// Vertex + Index buffer combo
struct MeshBuffers
{
  SDL_GPUBuffer* VertexBuffer{};
  SDL_GPUBuffer* IndexBuffer{};
};

struct MeshAsset
{
  const char* Name;
  std::vector<Geometry> Submeshes;

  MeshBuffers Buffers{};
  SDL_GPUBuffer* VertexBuffer() const { return Buffers.VertexBuffer; }
  SDL_GPUBuffer* IndexBuffer() const { return Buffers.IndexBuffer; }
};

class GLTFLoader
{
public:
  GLTFLoader(Program* program, std::filesystem::path path);
  ~GLTFLoader();

  bool Load();
  const std::vector<MeshAsset>& Meshes() const;
  const std::vector<SDL_GPUTexture*>& Textures() const;
  const std::vector<SDL_GPUSampler*>& Samplers() const;
  const std::vector<SharedPtr<PbrMaterial>>& Materials() const;
  void Release();

private:
  bool LoadVertexData();
  // send buffer data to GPU
  template<typename V, typename I>
  bool UploadBuffers(MeshBuffers* buffers,
                     std::vector<V> vertices,
                     std::vector<I> indices);

  bool LoadImageData();
  void LoadImageFromURI(LoadedImage& img, const fastgltf::sources::URI& URI);
  void LoadImageFromVector(LoadedImage& img,
                           const fastgltf::sources::Vector& vector);
  void LoadImageFromBufferView(LoadedImage& img,
                               const fastgltf::BufferView& view,
                               const fastgltf::Buffer& buffer);
  SDL_GPUTexture* CreateAndUploadTexture(LoadedImage& img);
  bool CreateDefaultTexture();

  bool LoadSamplers();
  bool CreateDefaultSampler();

  bool LoadMaterials();
  void CreateDefaultMaterial();

private:
  Program* program_; // TODO: decouple program from renderer, and hold pointer
                     // to renderer instead
  fastgltf::Asset asset_;
  std::filesystem::path path_;
  bool loaded_{ false };

  SDL_GPUSampler* default_sampler_{ nullptr };
  SDL_GPUTexture* default_texture_{ nullptr };
  SharedPtr<PbrMaterial> default_material_{ nullptr };

  std::vector<MeshAsset> meshes_;
  std::vector<SDL_GPUTexture*> textures_;
  std::vector<SDL_GPUSampler*> samplers_;
  std::vector<SharedPtr<PbrMaterial>> materials_;
};

template<typename V, typename I>
bool
GLTFLoader::UploadBuffers(MeshBuffers* buffers,
                          std::vector<V> vertices,
                          std::vector<I> indices)
{
  LOG_TRACE("GLTFLoader::UploadBuffers");
  auto vert_count = vertices.size();
  auto idx_count = indices.size();
  auto Device = program_->Device;
  LOG_DEBUG("Mesh has {} vertices and {} indices", vert_count, idx_count);

  SDL_GPUBufferCreateInfo vertInfo{};
  {
    vertInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    vertInfo.size = static_cast<Uint32>(sizeof(V) * vert_count);
  }

  SDL_GPUBufferCreateInfo idxInfo{};
  {
    idxInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    idxInfo.size = static_cast<Uint32>(sizeof(I) * idx_count);
  }

  SDL_GPUTransferBufferCreateInfo transferInfo{};
  {
    Uint32 sz = sizeof(V) * vert_count + sizeof(I) * idx_count;
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferInfo.size = sz;
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

  SDL_GPUTransferBuffer* trBuf =
    SDL_CreateGPUTransferBuffer(Device, &transferInfo);

  if (!trBuf) {
    LOG_ERROR("couldn't create buffers");
    SDL_ReleaseGPUBuffer(Device, vbuf);
    SDL_ReleaseGPUBuffer(Device, ibuf);
    return false;
  }

#define RELEASE_BUFFERS                                                        \
  SDL_ReleaseGPUBuffer(Device, vbuf);                                          \
  SDL_ReleaseGPUBuffer(Device, ibuf);                                          \
  SDL_ReleaseGPUTransferBuffer(Device, trBuf);

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
    reg.size = static_cast<Uint32>(sizeof(V) * vert_count);
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
    SDL_ReleaseGPUTransferBuffer(Device, trBuf);
  }
  return ret;
#undef RELEASE_BUFFERS
}
