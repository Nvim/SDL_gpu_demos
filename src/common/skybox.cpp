#include <pch.h>

#include "common/cubemap.h"
#include "common/engine.h"
#include "common/pipeline_builder.h"
#include "common/rendersystem.h"
#include "common/types.h"
#include "common/unit_cube.h"
#include "skybox.h"

Skybox::Skybox(const std::filesystem::path path,
               Engine* engine,
               SDL_GPUTextureFormat framebuffer_format)
  : path_{ path }
  , engine_{ engine }
  , framebuffer_format_{ framebuffer_format }
{
  if (!Init()) {
    LOG_ERROR("Skybox initialization failed");
  }
}

Skybox::Skybox(const std::filesystem::path path,
               const char* vert_path,
               const char* frag_path,
               Engine* engine,
               SDL_GPUTextureFormat framebuffer_format)
  : VertPath{ vert_path }
  , FragPath{ frag_path }
  , path_{ path }
  , engine_{ engine }
  , framebuffer_format_{ framebuffer_format }
{
  if (!Init()) {
    LOG_ERROR("Skybox initialization failed");
  }
}

Skybox::~Skybox()
{
  LOG_TRACE("Destroying Skybox");
  auto* Device = engine_->Device;
  RELEASE_IF(Pipeline, SDL_ReleaseGPUGraphicsPipeline);
}

bool
Skybox::Init()
{
  LOG_TRACE("Skybox::Init");

  if (!std::filesystem::exists(path_)) {
    LOG_ERROR("Invalid path: {}", path_.c_str());
    return false;
  }

  // TODO: store loader instancees in engine
  ICubemapLoader* loader;
  if (std::filesystem::is_directory(path_)) {
    loader = &engine_->MultifileCubemapLoader;
  } else if (path_.extension().compare(".hdr") == 0) {
    loader = &engine_->ProjectionCubemapLoader;
  } else {
    loader = &engine_->KtxCubemapLoader;
  }

  Cubemap = loader->Load(path_, CubeMapUsage::Skybox);
  if (Cubemap == nullptr) {
    LOG_ERROR("couldn't load skybox textures");
    return false;
  }

  if (!CreatePipeline()) {
    LOG_ERROR("Couldn't create skybox pipeline");
    return false;
  }

  SDL_GPUSamplerCreateInfo samplerInfo{};
  {
    samplerInfo.min_filter = SDL_GPU_FILTER_NEAREST;
    samplerInfo.mag_filter = SDL_GPU_FILTER_NEAREST;
    samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    samplerInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  }
  CubemapSampler = SDL_CreateGPUSampler(engine_->Device, &samplerInfo);
  if (!CubemapSampler) {
    LOG_ERROR("Couldn't create sampler");
    return false;
  }

  if (!engine_->CreateAndUploadMeshBuffers(&Buffers,
                                           UnitCube::Verts,
                                           UnitCube::VertCount,
                                           UnitCube::Indices,
                                           UnitCube::IndexCount)) {
    LOG_ERROR("Couldn't upload vertex data");
    return false;
  }

  LOG_INFO("Initialized skybox");
  loaded_ = true;
  return loaded_;
}

bool
Skybox::CreatePipeline()
{
  LOG_TRACE("Skybox::CreatePipeline");
  auto vert = LoadShader(VertPath, engine_->Device, 0, 1, 0, 0);
  if (vert == nullptr) {
    LOG_ERROR("Couldn't load vertex shader at path {}", VertPath);
    return false;
  }
  auto frag = LoadShader(FragPath, engine_->Device, 1, 1, 0, 0);
  if (frag == nullptr) {
    LOG_ERROR("Couldn't load fragment shader at path {}", FragPath);
    return false;
  }

  auto depth_state = SDL_GPUDepthStencilState{};
  {
    depth_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
    depth_state.enable_depth_test = true;
    depth_state.enable_depth_write = false;
    depth_state.enable_stencil_test = false;
  }

  PipelineBuilder builder{};
  builder //
    .AddColorTarget(framebuffer_format_, false)
    .SetVertexShader(vert)
    .SetFragmentShader(frag)
    .SetPrimitiveType(SDL_GPU_PRIMITIVETYPE_TRIANGLELIST)
    .AddVertexAttribute(SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3)
    .EnableDepthWrite()
    .SetDepthStencilState(depth_state);
  Pipeline = builder.Build(engine_->Device);

  SDL_ReleaseGPUShader(engine_->Device, vert);
  SDL_ReleaseGPUShader(engine_->Device, frag);

  auto ret = Pipeline != nullptr;
  if (ret) {
    LOG_DEBUG("Created skybox pipeline");
  } else {
    LOG_ERROR("Couldn't create skybox pipeline: {}", GETERR);
  }
  return ret;
}

void
Skybox::Draw(SDL_GPUCommandBuffer* cmd_buf,
             SDL_GPURenderPass* pass,
             const CameraBinding& camera_uniform) const
{
  static const SDL_GPUTextureSamplerBinding texBind{ Cubemap->Texture,
                                                     CubemapSampler };
  static const SDL_GPUBufferBinding vBufBind{ Buffers.VertexBuffer, 0 };
  static const SDL_GPUBufferBinding iBufBind{ Buffers.IndexBuffer, 0 };

  SDL_BindGPUGraphicsPipeline(pass, Pipeline);
  SDL_PushGPUVertexUniformData(
    cmd_buf, 0, &camera_uniform, sizeof(CameraBinding));
  SDL_BindGPUVertexBuffers(pass, 0, &vBufBind, 1);
  SDL_BindGPUIndexBuffer(pass, &iBufBind, SDL_GPU_INDEXELEMENTSIZE_16BIT);
  SDL_BindGPUFragmentSamplers(pass, 0, &texBind, 1);
  SDL_DrawGPUIndexedPrimitives(pass, 36, 1, 0, 0, 0);
}
