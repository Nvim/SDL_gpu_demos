#include <pch.h>

#include <SDL3_image/SDL_image.h>

SDL_GPUShader*
LoadShader(const char* path,
           SDL_GPUDevice* device,
           Uint32 samplerCount,
           Uint32 uniformBufferCount,
           Uint32 storageBufferCount,
           Uint32 storageTextureCount)
{
  SDL_GPUShaderStage stage;
  if (SDL_strstr(path, "vert")) {
    stage = SDL_GPU_SHADERSTAGE_VERTEX;
  } else if (SDL_strstr(path, "frag")) {
    stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
  } else {
    LOG_ERROR("Invalid shader stage!");
    return NULL;
  }
  size_t codeSize;
  void* code = SDL_LoadFile(path, &codeSize);
  if (code == NULL) {
    LOG_ERROR("Couldn't load shader code from path: {}", path);
    return NULL;
  }

  SDL_GPUShaderCreateInfo shaderInfo = {
    .code_size = codeSize,
    .code = (Uint8*)code,
    .entrypoint = "main",
    .format = SDL_GPU_SHADERFORMAT_SPIRV,
    .stage = stage,
    .num_samplers = samplerCount,
    .num_storage_textures = storageTextureCount,
    .num_storage_buffers = storageBufferCount,
    .num_uniform_buffers = uniformBufferCount,
    .props = 0,
  };

  SDL_GPUShader* shader = SDL_CreateGPUShader(device, &shaderInfo);
  if (shader == NULL) {
    LOG_ERROR("Failed to create shader: {}", GETERR);
    SDL_free(code);
    return NULL;
  }

  LOG_DEBUG("Created shader from path: {}", path);
  SDL_free(code);
  return shader;
}

SDL_Surface*
LoadImage(const char* path)
{
  SDL_Surface* result;
  SDL_PixelFormat format;

  result = IMG_Load(path);
  if (result == NULL) {
    return NULL;
  }

  format = SDL_PIXELFORMAT_ABGR8888;
  if (result->format != format) {
    SDL_Surface* next = SDL_ConvertSurface(result, format);
    SDL_DestroySurface(result);
    result = next;
  }

  LOG_DEBUG("Created surface from image: {}", path);
  return result;
}
