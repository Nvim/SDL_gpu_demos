#include "cubemap.h"
#include "common/loaded_image.h"
#include "common/logger.h"
#include "common/pipeline_builder.h"
#include "common/types.h"
#include "common/unit_cube.h"

#include <SDL3/SDL_gpu.h>
#include <pch.h>

#include <ktx.h>
#include <stb/stb_image.h>

Cubemap::~Cubemap()
{
  LOG_TRACE("Destroying cubemap");
  if (Texture != nullptr) {
    SDL_ReleaseGPUTexture(device_, Texture);
  }
}

MultifileCubemapLoader::MultifileCubemapLoader(SDL_GPUDevice* device)
  : device_{ device }
{
}

UniquePtr<Cubemap>
MultifileCubemapLoader::Load(std::filesystem::path dir,
                             CubeMapUsage usage) const
{
  LOG_TRACE("MultifileCubeMapLoader::Load");
  std::array<LoadedImage, 6> imgs;

  { // Load all faces in memory
    for (u8 i = 0; i < 6; ++i) {
      if (!ImageLoader::Load(imgs[i], dir / paths_[i])) {
        LOG_ERROR("couldn't load cubemap texture #{}: {}", i, GETERR);
        return nullptr;
      }
      // assert(img.nrChannels == 4);
    }
  }

  SDL_GPUTextureFormat format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  auto ret = MakeUnique<Cubemap>();
  {
    ret->Path = dir;
    ret->Usage = usage;
    ret->Format = format;
    ret->device_ = device_;
  }
  u32 width = imgs[0].w;
  u32 height = imgs[0].h;
  u32 imgSz = 4 * width * height;

  SDL_GPUTransferBufferCreateInfo info{};
  {
    info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    info.size = imgSz * 6;
  };

  SDL_GPUTransferBuffer* trBuf = SDL_CreateGPUTransferBuffer(device_, &info);
  if (!trBuf) {
    LOG_ERROR("couldn't create GPU transfer buffer: {}", GETERR);
    return nullptr;
  }

  { // Map transfer buffer and memcpy data
    u8* textureTransferPtr =
      (u8*)SDL_MapGPUTransferBuffer(device_, trBuf, false);
    if (!textureTransferPtr) {
      SDL_ReleaseGPUTransferBuffer(device_, trBuf);
      LOG_ERROR("couldn't get transfer buffer mapping: {}", GETERR);
      return nullptr;
    }
    for (int i = 0; i < 6; ++i) {
      const auto& img = imgs[i];
      SDL_memcpy(textureTransferPtr + (imgSz * i), img.data, imgSz);
    }
    SDL_UnmapGPUTransferBuffer(device_, trBuf);
  }

  { // Create cubemap with right dimensions now that we got image size
    SDL_GPUTextureCreateInfo cubeMapInfo{};
    {
      cubeMapInfo.type = SDL_GPU_TEXTURETYPE_CUBE;
      cubeMapInfo.format = format;
      cubeMapInfo.width = width;
      cubeMapInfo.height = height;
      cubeMapInfo.layer_count_or_depth = 6;
      cubeMapInfo.num_levels = 1;
      // TODO: future usages may require different flags:
      cubeMapInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    };
    ret->Texture = SDL_CreateGPUTexture(device_, &cubeMapInfo);
    if (!ret->Texture) {
      LOG_ERROR("couldn't create cubemap texture: {}", GETERR);
      SDL_ReleaseGPUTransferBuffer(device_, trBuf);
      return nullptr;
    }
  }

  { // Copy pass transfer buffer to GPU
    SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(device_);
    if (!cmdbuf) {
      LOG_ERROR("couldn't get command buffer for copy pass: {}", GETERR);
      SDL_ReleaseGPUTransferBuffer(device_, trBuf);
      return nullptr;
    }

    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdbuf);

    for (u32 i = 0; i < 6; i += 1) {
      SDL_GPUTextureTransferInfo trInfo{};
      {
        trInfo.transfer_buffer = trBuf;
        trInfo.offset = imgSz * i;
      }
      SDL_GPUTextureRegion texReg{};
      {
        texReg.texture = ret->Texture;
        texReg.layer = i;
        texReg.w = width;
        texReg.h = height;
        texReg.d = 1;
      };
      SDL_UploadToGPUTexture(copyPass, &trInfo, &texReg, false);
    }
    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(cmdbuf);
  }

  SDL_ReleaseGPUTransferBuffer(device_, trBuf);
  LOG_DEBUG("Loaded cubemap `{}` textures", dir.c_str());

  return ret;
}

KtxCubemapLoader::KtxCubemapLoader(SDL_GPUDevice* device)
  : device_{ device }
{
}

UniquePtr<Cubemap>
KtxCubemapLoader::Load(std::filesystem::path path, CubeMapUsage usage) const
{
  LOG_TRACE("MultifileCubeMapLoader::Load");

#define ERR ktxErrorString(result)
  ktxTexture* texture;
  KTX_error_code result;
  std::array<u8*, 6> imgs;

  result = ktxTexture_CreateFromNamedFile(
    path.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture);

  if (result != KTX_SUCCESS) {
    LOG_ERROR("Couldn't load KTX texture {}: {}", path.c_str(), ERR)
    return nullptr;
  }

  if (!texture->isCubemap || texture->numFaces != 6) {
    LOG_ERROR("Texture {} isn't a cubemap", path.c_str());
    ktxTexture_Destroy(texture);
    return nullptr;
  }

  LOG_DEBUG("layers: {}, levels: {}", texture->numLayers, texture->numLevels);

  { // Load texture data to cpu
    for (u8 i = 0; i < 6; i++) {
      u64 offset;
      // level = mip level = 1 -> num_levels
      // layer = index in array in case of texture array. only 1 cubemap -> 0
      result = ktxTexture_GetImageOffset(texture, 0, 0, i, &offset);
      if (result != KTX_SUCCESS) {
        LOG_ERROR("Couldn't get offset for face #{}: {}", i, ERR)
        ktxTexture_Destroy(texture);
        return nullptr;
      }
      imgs[i] = ktxTexture_GetData(texture) + offset;
    }
  }

  u32 width = texture->baseWidth;
  u32 height = texture->baseHeight;
  auto face_sz = ktxTexture_GetLevelSize(texture, 0) / 6;
  auto f = reinterpret_cast<ktxTexture2*>(texture)->vkFormat;
  if (!VkToSDL_TextureFormat.contains(f)) {
    LOG_ERROR("unknown vulkan format");
    ktxTexture_Destroy(texture);
    return nullptr;
  }
  auto format = VkToSDL_TextureFormat.at(f);

  auto ret = MakeUnique<Cubemap>();
  {
    ret->Path = path;
    ret->Usage = usage;
    ret->Format = format;
    ret->device_ = device_;
  }

  SDL_GPUTransferBufferCreateInfo info{};
  {
    info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    info.size = texture->dataSize;
  };

  SDL_GPUTransferBuffer* trBuf = SDL_CreateGPUTransferBuffer(device_, &info);
  if (!trBuf) {
    ktxTexture_Destroy(texture);
    LOG_ERROR("couldn't create GPU transfer buffer: {}", GETERR);
    return nullptr;
  }

  { // Map transfer buffer and memcpy data
    u8* textureTransferPtr =
      (u8*)SDL_MapGPUTransferBuffer(device_, trBuf, false);
    if (!textureTransferPtr) {
      ktxTexture_Destroy(texture);
      SDL_ReleaseGPUTransferBuffer(device_, trBuf);
      LOG_ERROR("couldn't get transfer buffer mapping: {}", GETERR);
      return nullptr;
    }
    for (int i = 0; i < 6; ++i) {
      SDL_memcpy(textureTransferPtr + (face_sz * i), imgs[i], face_sz);
    }
    SDL_UnmapGPUTransferBuffer(device_, trBuf);
  }

  { // Create cubemap
    SDL_GPUTextureCreateInfo cubeMapInfo{};
    {
      cubeMapInfo.type = SDL_GPU_TEXTURETYPE_CUBE;
      cubeMapInfo.format = format;
      cubeMapInfo.width = width;
      cubeMapInfo.height = height;
      cubeMapInfo.layer_count_or_depth = 6;
      cubeMapInfo.num_levels = 1;
      // TODO: future usages may require different flags:
      cubeMapInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    };
    ret->Texture = SDL_CreateGPUTexture(device_, &cubeMapInfo);
    if (!ret->Texture) {
      LOG_ERROR("couldn't create cubemap texture: {}", GETERR);
      SDL_ReleaseGPUTransferBuffer(device_, trBuf);
      return nullptr;
    }
  }

  { // Copy pass transfer buffer to GPU
    SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(device_);
    if (!cmdbuf) {
      LOG_ERROR("couldn't get command buffer for copy pass: {}", GETERR);
      SDL_ReleaseGPUTransferBuffer(device_, trBuf);
      return nullptr;
    }

    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdbuf);

    for (u32 i = 0; i < 6; i++) {
      SDL_GPUTextureTransferInfo trInfo{};
      {
        trInfo.transfer_buffer = trBuf;
        trInfo.offset = face_sz * i;
      }
      SDL_GPUTextureRegion texReg{};
      {
        texReg.texture = ret->Texture;
        texReg.layer = i;
        texReg.w = width;
        texReg.h = height;
        texReg.d = 1;
      };
      SDL_UploadToGPUTexture(copyPass, &trInfo, &texReg, false);
    }
    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(cmdbuf);
  }

  ktxTexture_Destroy(texture);
  SDL_ReleaseGPUTransferBuffer(device_, trBuf);
  LOG_DEBUG("Loaded cubemap `{}` KTX texture", path.c_str());

  return ret;
}

ProjectionCubemapLoader::ProjectionCubemapLoader(SDL_GPUDevice* device)
  : device_{ device }
{
  if (!CreatePipeline()) {
    LOG_ERROR("Couldn't create pipeline");
    return;
  }

  if (!UploadVertexData()) {
    LOG_ERROR("Couldn't send vertex data");
    return;
  }

  init_ = true;
}

ProjectionCubemapLoader::~ProjectionCubemapLoader()
{
  auto* Device = this->device_;
  RELEASE_IF(VertexBuffer, SDL_ReleaseGPUBuffer);
  RELEASE_IF(IndexBuffer, SDL_ReleaseGPUBuffer);
  RELEASE_IF(Pipeline, SDL_ReleaseGPUGraphicsPipeline);
}

UniquePtr<Cubemap>
ProjectionCubemapLoader::Load(std::filesystem::path path,
                              CubeMapUsage usage) const
{
  LOG_TRACE("ProjectionCubeMapLoader::Load");

  if (!init_) {
    LOG_ERROR("ProjectionCubemapLoader isn't initialized");
    return nullptr;
  }

  if (usage != CubeMapUsage::Skybox) {
    LOG_ERROR("HDR equirectangular maps are only supported for skyboxes");
    return nullptr;
  }

  LoadedImage img{};
  constexpr auto tex_format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
  SDL_GPUTexture* tex{ nullptr };
  { // Load texture data to cpu and create texture
    auto type = ImageType::DIMENSIONS_2D;
    auto f = ImagePixelFormat::PIXELFORMAT_FLOAT;
    if (!ImageLoader::Load(img, path, type, f)) {
      LOG_ERROR("couldn't load cubemap image {}", path.c_str());
      return nullptr;
    }
    SDL_GPUTextureCreateInfo tex_info{};
    {
      tex_info.type = SDL_GPU_TEXTURETYPE_2D;
      tex_info.format = tex_format, tex_info.height = img.h;
      tex_info.width = img.w;
      tex_info.layer_count_or_depth = 1;
      tex_info.num_levels = 1;
      tex_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    }
    tex = SDL_CreateGPUTexture(device_, &tex_info);
    if (!tex) {
      LOG_ERROR("Couldn't create texture: {}", GETERR);
      return nullptr;
    }
  }

  SDL_GPUSampler* tex_sampler{};
  {
    SDL_GPUSamplerCreateInfo samplerInfo{};
    {
      samplerInfo.min_filter = SDL_GPU_FILTER_LINEAR;
      samplerInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
      samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
      samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
      samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    }
    tex_sampler = SDL_CreateGPUSampler(device_, &samplerInfo);
    if (!tex_sampler) {
      LOG_ERROR("Couldn't create sampler, {}", GETERR);
      return nullptr;
    }
  }

  SDL_GPUTransferBuffer* trBuf;
  { // Memcpy data from cpu -> transfer buffer
    SDL_GPUTransferBufferCreateInfo info{};
    {
      info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
      info.size = img.DataSize();
    };
    trBuf = SDL_CreateGPUTransferBuffer(device_, &info);
    if (!trBuf) {
      SDL_ReleaseGPUTexture(device_, tex);
      LOG_ERROR("couldn't create GPU transfer buffer: {}", GETERR);
      return nullptr;
    }
    u8* mapped = (u8*)SDL_MapGPUTransferBuffer(device_, trBuf, false);
    if (!mapped) {
      SDL_ReleaseGPUTexture(device_, tex);
      SDL_ReleaseGPUTransferBuffer(device_, trBuf);
      LOG_ERROR("couldn't get transfer buffer mapping: {}", GETERR);
      return nullptr;
    }
    SDL_memcpy(mapped, img.data, img.DataSize());
    SDL_UnmapGPUTransferBuffer(device_, trBuf);
  }

  SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(device_);
  if (cmdbuf == NULL) {
    LOG_ERROR("Couldn't acquire command buffer: {}", GETERR);
    return nullptr;
  }

  { // Copy pass transfer buffer -> GPU texture
    SDL_GPUTextureTransferInfo tex_transfer_info{};
    {
      tex_transfer_info.transfer_buffer = trBuf;
      tex_transfer_info.offset = 0;
    }
    SDL_GPUTextureRegion tex_reg{};
    {
      tex_reg.texture = tex;
      tex_reg.w = img.w;
      tex_reg.h = img.h;
      tex_reg.d = 1;
    }
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdbuf);
    SDL_UploadToGPUTexture(copyPass, &tex_transfer_info, &tex_reg, false);

    SDL_EndGPUCopyPass(copyPass);
  }
  SDL_ReleaseGPUTransferBuffer(device_, trBuf);

  constexpr u32 width = 512, height = 512;
  auto ret = MakeUnique<Cubemap>();
  {
    ret->Path = path;
    ret->Usage = usage;
    ret->Format = tex_format;
    ret->device_ = device_;
  }

  { // Create cubemap
    SDL_GPUTextureCreateInfo cubeMapInfo{};
    {
      cubeMapInfo.type = SDL_GPU_TEXTURETYPE_CUBE;
      cubeMapInfo.format = tex_format;
      cubeMapInfo.width = width;
      cubeMapInfo.height = height;
      cubeMapInfo.layer_count_or_depth = 6;
      cubeMapInfo.num_levels = 1;
      cubeMapInfo.usage =
        SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    };
    ret->Texture = SDL_CreateGPUTexture(device_, &cubeMapInfo);
    if (!ret->Texture) {
      LOG_ERROR("Couldn't create cubemap texture: {}", GETERR);
      return nullptr;
    }
  }

  { // Draw unit cube & output to each face
    SDL_GPUColorTargetInfo target_info;
    {
      target_info.clear_color = { 0.8f, 0.1f, 0.8f, 1.0f };
      target_info.load_op = SDL_GPU_LOADOP_CLEAR;
      target_info.store_op = SDL_GPU_STOREOP_STORE;
      target_info.resolve_texture = nullptr;
      target_info.mip_level = 0;
    }
    static const SDL_GPUViewport viewport{ 0, 0, 512.f, 512.f, .1f, 1.f };
    const SDL_GPUBufferBinding vert_bind{ VertexBuffer, 0 };
    const SDL_GPUBufferBinding idx_bind{ IndexBuffer, 0 };
    const SDL_GPUTextureSamplerBinding sampler_bind{ tex, tex_sampler };
    static const glm::mat4 mat_proj =
      glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    static const glm::mat4 mat_views[] = {
      glm::lookAt(glm::vec3{ 0.f }, { 1.f, 0.f, 0.f }, { 0.f, -1.f, 0.f }),
      glm::lookAt(glm::vec3{ 0.f }, { -1.f, 0.f, 0.f }, { 0.f, -1.f, 0.f }),
      glm::lookAt(glm::vec3{ 0.f }, { 0.f, -1.f, 0.f }, { 0.f, 0.f, -1.f }),
      glm::lookAt(glm::vec3{ 0.f }, { 0.f, 1.f, 0.f }, { 0.f, 0.f, 1.f }),
      glm::lookAt(glm::vec3{ 0.f }, { 0.f, 0.f, 1.f }, { 0.f, -1.f, 0.f }),
      glm::lookAt(glm::vec3{ 0.f }, { 0.f, 0.f, -1.f }, { 0.f, -1.f, 0.f })
    };
    glm::mat4 uniform[2] = { mat_proj, mat_views[0] };

    for (u8 i = 0; i < 6; i++) {
      target_info.texture = ret->Texture;
      target_info.layer_or_depth_plane = i;
      SDL_GPURenderPass* pass =
        SDL_BeginGPURenderPass(cmdbuf, &target_info, 1, nullptr);

      SDL_SetGPUViewport(pass, &viewport);
      SDL_BindGPUGraphicsPipeline(pass, Pipeline);
      SDL_BindGPUVertexBuffers(pass, 0, &vert_bind, 1);
      SDL_BindGPUIndexBuffer(pass, &idx_bind, SDL_GPU_INDEXELEMENTSIZE_16BIT);
      SDL_BindGPUFragmentSamplers(pass, 0, &sampler_bind, 1);

      uniform[1] = mat_views[i];
      SDL_PushGPUVertexUniformData(cmdbuf, 0, &uniform, 2 * sizeof(glm::mat4));
      SDL_DrawGPUIndexedPrimitives(pass, UnitCube::IndexCount, 1, 0, 0, 0);

      SDL_EndGPURenderPass(pass);
    }
  }

  auto b = SDL_SubmitGPUCommandBuffer(cmdbuf);
  if (!b) {
    LOG_ERROR("Couldn't submit command buffer: {}", GETERR);
  } else {
    LOG_DEBUG("Loaded HDR texture `{}` as cubemap", path.c_str());
  }

  SDL_ReleaseGPUSampler(device_, tex_sampler);
  SDL_ReleaseGPUTexture(device_, tex);
  return ret;
}

bool
ProjectionCubemapLoader::CreatePipeline()
{
  LOG_TRACE("ProjectionCubemapLoader::CreatePipeline");
  auto vert = LoadShader(VertPath, device_, 0, 1, 0, 0);
  if (vert == nullptr) {
    LOG_ERROR("Couldn't load vertex shader at path {}", VertPath);
    return false;
  }
  auto frag = LoadShader(FragPath, device_, 1, 0, 0, 0);
  if (frag == nullptr) {
    LOG_ERROR("Couldn't load fragment shader at path {}", FragPath);
    return false;
  }

  PipelineBuilder builder{};
  builder //
    .AddColorTarget(SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT, false)
    .SetVertexShader(vert)
    .SetFragmentShader(frag)
    .SetPrimitiveType(SDL_GPU_PRIMITIVETYPE_TRIANGLELIST)
    .AddVertexAttribute(SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3);
  Pipeline = builder.Build(device_);

  SDL_ReleaseGPUShader(device_, vert);
  SDL_ReleaseGPUShader(device_, frag);

  auto ret = Pipeline != nullptr;
  if (ret) {
    LOG_DEBUG("Created hdr cubemap pipeline");
  } else {
    LOG_ERROR("Couldn't create hdr cubemap pipeline: {}", GETERR);
  }
  return ret;
}

bool
ProjectionCubemapLoader::UploadVertexData()
{
  LOG_TRACE("ProjectionCubemapLoader::UploadVertexData");

  const u32 vbuf_sz = sizeof(PosVertex) * UnitCube::VertCount;
  const u32 ibuf_sz = sizeof(u16) * UnitCube::IndexCount;
  const u32 trbuf_sz = vbuf_sz + ibuf_sz;
  SDL_GPUBufferCreateInfo bufInfo{};
  {
    bufInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    bufInfo.size = vbuf_sz;
  }
  VertexBuffer = SDL_CreateGPUBuffer(device_, &bufInfo);
  if (!VertexBuffer) {
    LOG_ERROR("Couldn't create vertex buffer: {}", GETERR);
    return false;
  }

  {
    bufInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    bufInfo.size = ibuf_sz;
  }
  IndexBuffer = SDL_CreateGPUBuffer(device_, &bufInfo);
  if (!VertexBuffer) {
    LOG_ERROR("Couldn't create index buffer: {}", GETERR);
    return false;
  }

  SDL_GPUTransferBufferCreateInfo trInfo{};
  {
    trInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    trInfo.size = trbuf_sz;
  }
  SDL_GPUTransferBuffer* trBuf = SDL_CreateGPUTransferBuffer(device_, &trInfo);
  if (!trBuf) {
    LOG_ERROR("Couldn't create transfer buffer: {}", GETERR);
    return false;
  }

  { // Transfer buffer
    PosVertex* transferData =
      (PosVertex*)SDL_MapGPUTransferBuffer(device_, trBuf, false);
    u16* indexData = (u16*)&transferData[24];

    SDL_memcpy(transferData, UnitCube::Verts, sizeof(UnitCube::Verts));
    SDL_memcpy(indexData, UnitCube::Indices, sizeof(UnitCube::Indices));
    SDL_UnmapGPUTransferBuffer(device_, trBuf);
  }

  SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(device_);

  { // Copy pass
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdbuf);

    SDL_GPUTransferBufferLocation trLoc{ trBuf, 0 };
    SDL_GPUBufferRegion vBufReg{ VertexBuffer, 0, vbuf_sz };
    SDL_GPUBufferRegion iBufReg{ IndexBuffer, 0, ibuf_sz };

    SDL_UploadToGPUBuffer(copyPass, &trLoc, &vBufReg, false);
    trLoc.offset = vBufReg.size;
    SDL_UploadToGPUBuffer(copyPass, &trLoc, &iBufReg, false);
    SDL_EndGPUCopyPass(copyPass);
  }

  SDL_ReleaseGPUTransferBuffer(device_, trBuf);
  auto ret = SDL_SubmitGPUCommandBuffer(cmdbuf);
  if (ret) {
    LOG_DEBUG("Sent cubemap vertex data to GPU");
  } else {
    LOG_ERROR("Couldn't submit command buffer for vertex transfer: {}", GETERR);
  }
  return ret;
}

bool
ProjectionCubemapLoader::CreateSampler()
{
  LOG_TRACE("ProjectionCubemapLoader::CreateSampler");

  return true;
}
