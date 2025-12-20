#include <pch.h>

#include "grid.h"

#include "common/pipeline_builder.h"
#include "common/types.h"
#include <SDL3/SDL_gpu.h>

Grid::Grid(Engine* engine, SDL_GPUTextureFormat framebuffer_format)
  : engine_{ engine }
  , framebuffer_format_{ framebuffer_format }
{
  LOG_TRACE("Grid::Grid");
  if (!Init()) {
    LOG_ERROR("Grid initialization failed");
  }
}

Grid::~Grid()
{
  LOG_TRACE("Destroying Grid");
  auto* Device = engine_->Device;
  RELEASE_IF(Pipeline, SDL_ReleaseGPUGraphicsPipeline);
}

bool
Grid::Init()
{
  auto vs = LoadShader(VERT_PATH, engine_->Device, 0, 1, 0, 0);
  if (vs == nullptr) {
    LOG_ERROR("Couldn't load vertex shader at path {}", VERT_PATH);
    return false;
  }
  auto fs = LoadShader(FRAG_PATH, engine_->Device, 0, 0, 0, 0);
  if (fs == nullptr) {
    LOG_ERROR("Couldn't load fragment shader at path {}", FRAG_PATH);
    return false;
  }

  PipelineBuilder builder{};

  Pipeline = builder //
               .AddColorTarget(framebuffer_format_, true)
               .SetFragmentShader(fs)
               .SetVertexShader(vs)
               .EnableDepthWrite()
               .EnableDepthTest()
               .SetCompareOp(SDL_GPU_COMPAREOP_LESS)
               .EnableDepthWrite()
               .Build(engine_->Device);
  SDL_ReleaseGPUShader(engine_->Device, vs);
  SDL_ReleaseGPUShader(engine_->Device, fs);

  loaded_ = Pipeline != nullptr;
  if (!loaded_) {
    LOG_ERROR("Couldn't create grid pipeline: {}", GETERR);
  } else {
    LOG_DEBUG("Created grid pipeline");
  }
  return loaded_;
}

void
Grid::Draw(SDL_GPUCommandBuffer* cmd_buf,
           SDL_GPURenderPass* pass,
           const CameraBinding& camera_uniform) const
{
  SDL_BindGPUGraphicsPipeline(pass, Pipeline);
  SDL_PushGPUVertexUniformData(
    cmd_buf, 0, &camera_uniform, sizeof(CameraBinding));
  SDL_DrawGPUIndexedPrimitives(pass, 6, 1, 0, 0, 0);
}
