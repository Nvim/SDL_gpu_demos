#pragma once

#include "common/types.h"
#include <SDL3/SDL_gpu.h>
#include <array>

// Only supports a single vertex buffer for now
// TODO: take ownership of shader pointers to avoid releasing them too early
// TODO: multisampling state support
// TODO: winding order & culling support
struct PipelineBuilder
{
  static constexpr u32 MAX_COLOR_TARGETS = 4;
  static constexpr u32 NUM_VERTEX_BUFFERS = 1;
  static constexpr SDL_GPUTextureFormat DEPTH_FORMAT =
    SDL_GPU_TEXTUREFORMAT_D16_UNORM;

  PipelineBuilder();

  [[nodiscard]] SDL_GPUGraphicsPipeline* Build(SDL_GPUDevice* device);
  void Reset();
  PipelineBuilder& AddColorTarget(SDL_GPUTextureFormat format, bool blend);
  PipelineBuilder& AddVertexAttribute(SDL_GPUVertexElementFormat format);
  PipelineBuilder& AddVertexAttributes(
    std::span<const SDL_GPUVertexElementFormat> formats);
  PipelineBuilder& SetVertexShader(SDL_GPUShader* vs);
  PipelineBuilder& SetFragmentShader(SDL_GPUShader* fs);
  PipelineBuilder& SetPrimitiveType(SDL_GPUPrimitiveType type);
  PipelineBuilder& EnableDepthWrite(SDL_GPUTextureFormat format = DEPTH_FORMAT);
  PipelineBuilder& EnableDepthTest();
  PipelineBuilder& EnableStencilTest();
  PipelineBuilder& SetCompareOp(SDL_GPUCompareOp compare_op);
  PipelineBuilder& SetDepthStencilState(
    SDL_GPUDepthStencilState state); // for full control

  std::array<SDL_GPUColorTargetDescription, MAX_COLOR_TARGETS> color_descs{};
  u32& num_color_targets{ pipeline_info.target_info.num_color_targets };
  SDL_GPUVertexBufferDescription vert_desc{};
  std::vector<SDL_GPUVertexAttribute> vertex_attributes{};
  u32 vertex_attrs_offset{ 0 };
  SDL_GPURasterizerState rasterizer_state{};
  SDL_GPUGraphicsPipelineCreateInfo pipeline_info{};
};
