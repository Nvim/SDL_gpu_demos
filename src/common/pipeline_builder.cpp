#include <pch.h>

#include "common/util.h"
#include "pipeline_builder.h"

PipelineBuilder::PipelineBuilder()
{
  Reset();
}

SDL_GPUGraphicsPipeline*
PipelineBuilder::Build(SDL_GPUDevice* device)
{
  pipeline_info.vertex_input_state.vertex_attributes = vertex_attributes.data();
  // pipeline_info.rasterizer_state = rasterizer_state;
  return SDL_CreateGPUGraphicsPipeline(device, &pipeline_info);
}

void
PipelineBuilder::Reset()
{
  pipeline_info = {};
  color_descs = {};
  num_color_targets = 0;
  vert_desc = {};
  vertex_attributes.clear();

  // TODO: configurable rasterizer state. I don't need it for now, but full GLTF
  // support will require it
  rasterizer_state = {};
  {
    rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
    rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
  }

  pipeline_info.target_info.color_target_descriptions = color_descs.data();
  {
    auto& state = pipeline_info.vertex_input_state;
    state.vertex_buffer_descriptions = &vert_desc;
    state.num_vertex_buffers = NUM_VERTEX_BUFFERS;
  }
}

PipelineBuilder&
PipelineBuilder::AddColorTarget(SDL_GPUTextureFormat format, bool blend)
{
  if (num_color_targets == MAX_COLOR_TARGETS) {
    LOG_WARN("Couldn't add color target: max limit reached");
    return *this;
  }
  SDL_GPUColorTargetDescription desc{};
  desc.format = format;
  if (blend) {
    enable_blending(desc);
  } else {
    disable_blending(desc);
  }
  color_descs[num_color_targets++] = desc;
  return *this;
}

PipelineBuilder&
PipelineBuilder::AddVertexAttribute(SDL_GPUVertexElementFormat format)
{
  SDL_GPUVertexAttribute attr{};
  {
    attr.buffer_slot = 0;
    attr.format = format;
    attr.location = vertex_attributes.size();
    attr.offset = vertex_attrs_offset;
  }
  vertex_attributes.push_back(attr);
  vertex_attrs_offset += vertex_attribute_size(format);
  vert_desc.pitch = vertex_attrs_offset;

  pipeline_info.vertex_input_state.num_vertex_attributes =
    vertex_attributes.size();

  return *this;
}

PipelineBuilder&
PipelineBuilder::AddVertexAttributes(
  std::span<const SDL_GPUVertexElementFormat> formats)
{
  for (const auto& format : formats) {
    AddVertexAttribute(format);
  }
  return *this;
}

PipelineBuilder&
PipelineBuilder::SetVertexShader(SDL_GPUShader* vs)
{
  pipeline_info.vertex_shader = vs;
  return *this;
}

PipelineBuilder&
PipelineBuilder::SetFragmentShader(SDL_GPUShader* fs)
{
  pipeline_info.fragment_shader = fs;
  return *this;
}

PipelineBuilder&
PipelineBuilder::SetPrimitiveType(SDL_GPUPrimitiveType type)
{
  pipeline_info.primitive_type = type;
  return *this;
}

PipelineBuilder&
PipelineBuilder::EnableDepthWrite(SDL_GPUTextureFormat format)
{
  pipeline_info.target_info.has_depth_stencil_target = true;
  pipeline_info.target_info.depth_stencil_format = format;
  pipeline_info.depth_stencil_state.enable_depth_write = true;
  return *this;
}

PipelineBuilder&
PipelineBuilder::EnableDepthTest()
{
  // pipeline_info.target_info.has_depth_stencil_target = true;
  pipeline_info.depth_stencil_state.enable_depth_test = true;
  return *this;
}

PipelineBuilder&
PipelineBuilder::EnableStencilTest()
{
  // pipeline_info.target_info.has_depth_stencil_target = true;
  pipeline_info.depth_stencil_state.enable_stencil_test = true;
  return *this;
}

PipelineBuilder&
PipelineBuilder::SetCompareOp(SDL_GPUCompareOp compare_op)
{
  pipeline_info.depth_stencil_state.compare_op = compare_op;
  return *this;
}

PipelineBuilder&
PipelineBuilder::SetDepthStencilState(SDL_GPUDepthStencilState state)
{
  pipeline_info.depth_stencil_state = state;
  return *this;
}
