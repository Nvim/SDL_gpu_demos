#include "cubemap.h"
#include "common/logger.h"
#include "common/types.h"

#include <pch.h>

#include <stb/stb_image.h>

Cubemap::~Cubemap()
{
  LOG_TRACE("Destroying cubemap");
  if (Texture != nullptr) {
    SDL_ReleaseGPUTexture(device_, Texture);
  }
}

MultifileCubeMapLoader::MultifileCubeMapLoader(SDL_GPUDevice* device)
  : device_{ device }
{
}

UniquePtr<Cubemap>
MultifileCubeMapLoader::Load(std::filesystem::path dir,
                             CubeMapUsage usage,
                             SDL_GPUTextureFormat format) const
{
  LOG_TRACE("MultifileCubeMapLoader::Load");
  std::array<LoadedImage, 6> imgs;

  { // Load all faces in memory
    for (u8 i = 0; i < 6; ++i) {
      auto& img = imgs[i];
      img.data = stbi_load(std::format("{}/{}", dir.c_str(), paths_[i]).c_str(),
                           &img.w,
                           &img.h,
                           &img.nrChannels,
                           4);
      if (!img.data) {
        LOG_ERROR("couldn't load skybox texture #{}: {}", i, GETERR);
        for (int j = 0; j < i; ++j) {
          stbi_image_free(imgs[j].data);
        }
        return nullptr;
      }
      // assert(img.nrChannels == 4);
    }
  }

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
  auto freeImg = [](LoadedImage& i) { stbi_image_free(i.data); };

  SDL_GPUTransferBufferCreateInfo info{};
  {
    info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    info.size = imgSz * 6;
  };

  SDL_GPUTransferBuffer* trBuf = SDL_CreateGPUTransferBuffer(device_, &info);
  if (!trBuf) {
    std::for_each(imgs.begin(), imgs.end(), freeImg);
    LOG_ERROR("couldn't create GPU transfer buffer: {}", GETERR);
    return nullptr;
  }

  { // Map transfer buffer and memcpy data
    u8* textureTransferPtr =
      (u8*)SDL_MapGPUTransferBuffer(device_, trBuf, false);
    if (!textureTransferPtr) {
      std::for_each(imgs.begin(), imgs.end(), freeImg);
      SDL_ReleaseGPUTransferBuffer(device_, trBuf);
      LOG_ERROR("couldn't get transfer buffer mapping: {}", GETERR);
      return nullptr;
    }
    for (int i = 0; i < 6; ++i) {
      const auto& img = imgs[i];
      SDL_memcpy(textureTransferPtr + (imgSz * i), img.data, imgSz);
      stbi_image_free(img.data); // don't need this anymore
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
  LOG_DEBUG("Loaded skybox `{}` textures", dir.c_str());

  return ret;
}
