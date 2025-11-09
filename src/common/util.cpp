#include <pch.h>

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
