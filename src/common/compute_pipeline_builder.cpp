#include <pch.h>

#include "common/logger.h"
#include "compute_pipeline_builder.h"

#include <SDL3/SDL_gpu.h>
#include <filesystem>

ComputePipelineBuilder::ComputePipelineBuilder()
{
  info_.entrypoint = "main";
  info_.format = SDL_GPU_SHADERFORMAT_SPIRV;
}

ComputePipelineBuilder::~ComputePipelineBuilder()
{
  if (code_) {
    SDL_free(code_);
  }
}

SDL_GPUComputePipeline*
ComputePipelineBuilder::Build(SDL_GPUDevice* device)
{
  if (code_ && code_size_) {
    info_.code = (u8*)code_;
    info_.code_size = code_size_;
    return SDL_CreateGPUComputePipeline(device, &info_);
  }

  if (!std::filesystem::exists(path_)) {
    LOG_ERROR("couldn't build compute pipeline: invalid path");
    return nullptr;
  }

  code_ = SDL_LoadFile(path_.c_str(), &code_size_);
  if (code_ == nullptr) {
    LOG_ERROR("Couldn't load compute shader file: {}", GETERR);
    return nullptr;
  }
  info_.code = (u8*)code_;
  info_.code_size = code_size_;
  return SDL_CreateGPUComputePipeline(device, &info_);
}

ComputePipelineBuilder&
ComputePipelineBuilder::SetReadOnlyStorageTextureCount(i32 count)
{
  info_.num_readonly_storage_textures = count;
  return *this;
}

ComputePipelineBuilder&
ComputePipelineBuilder::SetReadWriteStorageTextureCount(i32 count)
{
  info_.num_readwrite_storage_textures = count;
  return *this;
}

ComputePipelineBuilder&
ComputePipelineBuilder::SetReadOnlyStorageBufferCount(i32 count)
{
  info_.num_readonly_storage_buffers = count;
  return *this;
}

ComputePipelineBuilder&
ComputePipelineBuilder::SetReadWriteStorageBufferCount(i32 count)
{
  info_.num_readwrite_storage_buffers = count;
  return *this;
}

ComputePipelineBuilder&
ComputePipelineBuilder::SetUBOCount(i32 count)
{
  info_.num_uniform_buffers = count;
  return *this;
}

ComputePipelineBuilder&
ComputePipelineBuilder::SetThreadCount(i32 x, i32 y, i32 z)
{
  info_.threadcount_x = x;
  info_.threadcount_y = y;
  info_.threadcount_z = z;
  return *this;
}

ComputePipelineBuilder&
ComputePipelineBuilder::SetShader(std::filesystem::path path)
{
  path_ = path;
  if (code_) {
    SDL_free(code_);
    code_ = nullptr;
    code_size_ = 0;
  }
  return *this;
}

ComputePipelineBuilder&
ComputePipelineBuilder::SetShader(const char* path)
{
  path_ = std::filesystem::path{ path };
  if (code_) {
    SDL_free(code_);
    code_ = nullptr;
    code_size_ = 0;
  }
  return *this;
}
