#include <pch.h>

#include "common/cubemap.h"
#include "common/engine.h"
#include "common/rendersystem.h"
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

  SDL_GPUColorTargetDescription col_desc = {};
  col_desc.format = framebuffer_format_;

  SDL_GPUVertexBufferDescription vert_desc{};
  {
    vert_desc.slot = 0;
    vert_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    vert_desc.instance_step_rate = 0;
    vert_desc.pitch = sizeof(PosVertex);
  }

  SDL_GPUVertexAttribute vert_attr{};
  {
    vert_attr.buffer_slot = 0;
    vert_attr.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vert_attr.location = 0;
    vert_attr.offset = 0;
  }

  SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo = {};
  {
    pipelineCreateInfo.vertex_shader = vert;
    pipelineCreateInfo.fragment_shader = frag;
    pipelineCreateInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    {
      auto& state = pipelineCreateInfo.vertex_input_state;
      state.vertex_buffer_descriptions = &vert_desc;
      state.num_vertex_buffers = 1;
      state.vertex_attributes = &vert_attr;
      state.num_vertex_attributes = 1;
    }
    {
      auto& state = pipelineCreateInfo.target_info;
      state.color_target_descriptions = &col_desc;
      state.num_color_targets = 1;
      state.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
      state.has_depth_stencil_target = true;
    }
    {
      auto& state = pipelineCreateInfo.depth_stencil_state;
      state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
      state.enable_depth_test = true;
      state.enable_depth_write = false;
      state.enable_stencil_test = false;
    }
  }
  Pipeline =
    SDL_CreateGPUGraphicsPipeline(engine_->Device, &pipelineCreateInfo);

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
Skybox::Draw(SDL_GPURenderPass* pass) const
{
  static const SDL_GPUTextureSamplerBinding texBind{ Cubemap->Texture,
                                                     CubemapSampler };
  static const SDL_GPUBufferBinding vBufBind{ Buffers.VertexBuffer, 0 };
  static const SDL_GPUBufferBinding iBufBind{ Buffers.IndexBuffer, 0 };

  SDL_BindGPUGraphicsPipeline(pass, Pipeline);
  SDL_BindGPUVertexBuffers(pass, 0, &vBufBind, 1);
  SDL_BindGPUIndexBuffer(pass, &iBufBind, SDL_GPU_INDEXELEMENTSIZE_16BIT);
  SDL_BindGPUFragmentSamplers(pass, 0, &texBind, 1);
  SDL_DrawGPUIndexedPrimitives(pass, 36, 1, 0, 0, 0);
}
