#include "cubemap.h"
#include "common/loaded_image.h"
#include "common/logger.h"
#include "common/types.h"

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

KtxCubemapLoader::KtxCubemapLoader(SDL_GPUDevice* device)
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

    for (Uint32 i = 0; i < 6; i += 1) {
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
