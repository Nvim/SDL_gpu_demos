#pragma once

#include <filesystem>

class ComputePipelineBuilder
{

public:
  ComputePipelineBuilder();
  ~ComputePipelineBuilder();
  SDL_GPUComputePipeline* Build(SDL_GPUDevice* device);

  ComputePipelineBuilder& SetReadOnlyStorageTextureCount(i32 count);
  ComputePipelineBuilder& SetReadWriteStorageTextureCount(i32 count);
  ComputePipelineBuilder& SetReadOnlyStorageBufferCount(i32 count);
  ComputePipelineBuilder& SetReadWriteStorageBufferCount(i32 count);
  ComputePipelineBuilder& SetUBOCount(i32 count);
  ComputePipelineBuilder& SetThreadCount(i32 x, i32 y, i32 z);
  ComputePipelineBuilder& SetShader(std::filesystem::path path);
  ComputePipelineBuilder& SetShader(const char* path);

private:
  SDL_GPUComputePipelineCreateInfo info_{};
  std::filesystem::path path_;
  u64 code_size_{ 0 };
  void* code_{ nullptr };
};
