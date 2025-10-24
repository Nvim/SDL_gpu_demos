#include <pch.h>

#include "ssbo_demo.h"

BufferApp::BufferApp(SDL_GPUDevice* device, SDL_Window* window, int w, int h)
  : Program{ device, window }
  , vp_width_{ w }
  , vp_height_{ h }
{
  {
    swapchain_target_info_.clear_color = { 0.5f, 0.1f, 0.5f, 1.0f };
    swapchain_target_info_.load_op = SDL_GPU_LOADOP_CLEAR;
    swapchain_target_info_.store_op = SDL_GPU_STOREOP_STORE;
    swapchain_target_info_.mip_level = 0;
    swapchain_target_info_.layer_or_depth_plane = 0;
    swapchain_target_info_.cycle = false;
  }
}

BufferApp::~BufferApp()
{
  LOG_TRACE("Destroying app");
}

bool
BufferApp::Init()
{
  LOG_TRACE("BufferApp::Init");

  SDL_GPUShaderFormat backendFormats = SDL_GetGPUShaderFormats(Device);
  if (!(backendFormats & SDL_GPU_SHADERFORMAT_SPIRV)) {
    LOG_ERROR("Backend doesn't support SPRIR-V");
    return false;
  }

  if (!LoadShaders()) {
    LOG_ERROR("Couldn't load shaders");
    return false;
  }
  LOG_DEBUG("Loaded shaders");

  SDL_GPUColorTargetDescription color_descs[1]{};
  color_descs[0].format = SDL_GetGPUSwapchainTextureFormat(Device, Window);

  static constexpr size_t FLOAT3 = sizeof(glm::vec3);
  SDL_GPUVertexAttribute vertex_attributes[] = {
    { .location = 0,
      .buffer_slot = 0,
      .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
      .offset = 0 },
    { .location = 1,
      .buffer_slot = 0,
      .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
      .offset = FLOAT3 },
  };
  SDL_GPUVertexBufferDescription vertex_desc[] = { {
    .slot = 0,
    .pitch = sizeof(PosColVertex),
    .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
    .instance_step_rate = 0,
  } };

  SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo{};
  {
    pipelineCreateInfo.vertex_shader = vertex_;
    pipelineCreateInfo.fragment_shader = fragment_;
    pipelineCreateInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    {
      auto& state = pipelineCreateInfo.vertex_input_state;
      state.vertex_buffer_descriptions = vertex_desc;
      state.num_vertex_buffers = 1;
      state.vertex_attributes = vertex_attributes;
      state.num_vertex_attributes = 2;
    }
    {
      auto& state = pipelineCreateInfo.rasterizer_state;
      state.fill_mode = SDL_GPU_FILLMODE_FILL,
      state.cull_mode = SDL_GPU_CULLMODE_NONE;
      state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    }
    {
      auto& state = pipelineCreateInfo.depth_stencil_state;
      state.enable_depth_test = false;
      state.enable_depth_write = false;
      state.enable_stencil_test = false;
    }
    {
      auto& info = pipelineCreateInfo.target_info;
      info.color_target_descriptions = color_descs;
      info.num_color_targets = 1;
      info.has_depth_stencil_target = false;
    }
  }

  ScenePipeline = SDL_CreateGPUGraphicsPipeline(Device, &pipelineCreateInfo);
  if (ScenePipeline == NULL) {
    LOG_ERROR("Couldn't create pipeline!");
    return false;
  }
  LOG_DEBUG("Created pipeline");

  LOG_DEBUG("Created render target textures");

  if (!SendVertexData()) {
    LOG_ERROR("Couldn't upload vertex data");
    return false;
  }
  LOG_DEBUG("Uploaded vertex data");

  LOG_INFO("Initialized application");
  return true;
}

bool
BufferApp::Poll()
{
  SDL_Event evt;
  while (SDL_PollEvent(&evt)) {
    if (evt.type == SDL_EVENT_QUIT) {
      quit = true;
    } else if (evt.type == SDL_EVENT_KEY_DOWN) {
      if (evt.key.key == SDLK_ESCAPE) {
        quit = true;
      }
    }
  }
  return true;
}

bool
BufferApp::ShouldQuit()
{
  return quit;
}

bool
BufferApp::Draw()
{
  static const SDL_GPUViewport scene_vp{
    0, 0, float(vp_width_), float(vp_height_), 0.1f, 1.0f
  };
  static const SDL_GPUBufferBinding iBinding{ index_buffer_, 0 };
  const SDL_GPUBufferBinding vBinding{ vertex_buffer_, 0 };
  SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(Device);
  if (cmdbuf == NULL) {
    LOG_ERROR("Couldn't acquire command buffer: {}", SDL_GetError());
    return false;
  }

  SDL_GPUTexture* swapchainTexture;
  if (!SDL_WaitAndAcquireGPUSwapchainTexture(
        cmdbuf, Window, &swapchainTexture, NULL, NULL)) {
    LOG_ERROR("Couldn't acquire swapchain texture: {}", SDL_GetError());
    return false;
  }
  if (swapchainTexture == NULL) {
    SDL_SubmitGPUCommandBuffer(cmdbuf);
    return true;
  }

  // swapchain_target_info_.texture = swapchainTexture;
  //
  // SDL_GPURenderPass* pass =
  //   SDL_BeginGPURenderPass(cmdbuf, &swapchain_target_info_, 1, nullptr);
  // SDL_SetGPUViewport(pass, &scene_vp);
  // SDL_BindGPUGraphicsPipeline(pass, ScenePipeline);
  // SDL_BindGPUIndexBuffer(pass, &iBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
  // SDL_BindGPUVertexBuffers(pass, 0, &vBinding, 1);
  // // SDL_BindGPUVertexStorageBuffers(pass, 0, &ssbo_, 1);
  // SDL_DrawGPUIndexedPrimitives(pass, VERT_COUNT, 1, 0, 0, 0);
  // SDL_EndGPURenderPass(pass);
  SDL_GPUColorTargetInfo colorTargetInfo{};
  colorTargetInfo.texture = swapchainTexture;
  colorTargetInfo.clear_color = { 0.2f, 0.2f, 0.2f, 1.0f };
  colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
  colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;

  SDL_GPURenderPass* renderPass =
    SDL_BeginGPURenderPass(cmdbuf, &colorTargetInfo, 1, NULL);

  SDL_BindGPUGraphicsPipeline(renderPass, ScenePipeline);
  SDL_BindGPUVertexBuffers(renderPass, 0, &vBinding, 1);
  SDL_BindGPUIndexBuffer(renderPass, &iBinding, SDL_GPU_INDEXELEMENTSIZE_16BIT);
  SDL_BindGPUVertexStorageBuffers(renderPass, 0, &ssbo_, 1);
  SDL_SetGPUViewport(renderPass, &scene_vp);
  SDL_DrawGPUIndexedPrimitives(renderPass, INDEX_COUNT, 1, 0, 0, 0);
  SDL_EndGPURenderPass(renderPass);

  SDL_SubmitGPUCommandBuffer(cmdbuf);

  return true;
}

bool
BufferApp::LoadShaders()
{
  LOG_TRACE("BufferApp::LoadShaders");
  vertex_ = LoadShader(vertex_path_, Device, 0, 0, 1, 0);
  if (vertex_ == nullptr) {
    LOG_ERROR("Couldn't load vertex shader at path {}", vertex_path_);
    return false;
  }
  fragment_ = LoadShader(fragment_path_, Device, 0, 0, 0, 0);
  if (fragment_ == nullptr) {
    LOG_ERROR("Couldn't load fragment shader at path {}", fragment_path_);
    return false;
  }
  return true;
}

bool
BufferApp::SendVertexData()
{
  LOG_TRACE("BufferApp::SendVertexData");

#define VBUF_SZ sizeof(PosColVertex) * VERT_COUNT
#define IBUF_SZ sizeof(u16) * INDEX_COUNT
#define SSBO_SZ sizeof(PaddedVertex) * VERT_COUNT
  SDL_GPUBufferCreateInfo vert_info{};
  {
    vert_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    vert_info.size = VBUF_SZ;
  }

  SDL_GPUBufferCreateInfo idx_info{};
  {
    idx_info.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    idx_info.size = IBUF_SZ;
  }

  SDL_GPUBufferCreateInfo ssbo_info{};
  {
    ssbo_info.usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
    ssbo_info.size = SSBO_SZ;
  }

  SDL_GPUTransferBufferCreateInfo transferInfo{};
  {
    Uint32 sz = VBUF_SZ + IBUF_SZ + SSBO_SZ;
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferInfo.size = sz;
  }

  vertex_buffer_ = SDL_CreateGPUBuffer(Device, &vert_info);
  if (!vertex_buffer_) {
    LOG_ERROR("couldn't create storage buffer");
    return false;
  }

  index_buffer_ = SDL_CreateGPUBuffer(Device, &idx_info);
  if (!index_buffer_) {
    LOG_ERROR("couldn't create index buffer");
    SDL_ReleaseGPUBuffer(Device, vertex_buffer_);
    return false;
  }

  ssbo_ = SDL_CreateGPUBuffer(Device, &ssbo_info);
  if (!ssbo_) {
    return false;
  }

  SDL_GPUTransferBuffer* trBuf =
    SDL_CreateGPUTransferBuffer(Device, &transferInfo);

  if (!trBuf) {
    LOG_ERROR("couldn't create transfer buffer");
    SDL_ReleaseGPUBuffer(Device, vertex_buffer_);
    SDL_ReleaseGPUBuffer(Device, index_buffer_);
    SDL_ReleaseGPUBuffer(Device, ssbo_);
    return false;
  }
#define RELEASE_BUFFERS                                                        \
  SDL_ReleaseGPUBuffer(Device, vertex_buffer_);                                \
  SDL_ReleaseGPUBuffer(Device, index_buffer_);                                 \
  SDL_ReleaseGPUBuffer(Device, ssbo_);                                         \
  SDL_ReleaseGPUTransferBuffer(Device, trBuf);

  PosColVertex* transferData =
    (PosColVertex*)SDL_MapGPUTransferBuffer(Device, trBuf, false);
  if (!transferData) {
    LOG_ERROR("couldn't get mapping for transfer buffer");
    RELEASE_BUFFERS
    return false;
  }

  for (u32 i = 0; i < VERT_COUNT; ++i) {
    transferData[i] = verts[i];
  }

  u16* indexData = (u16*)&transferData[VERT_COUNT];
  for (u32 i = 0; i < INDEX_COUNT; ++i) {
    indexData[i] = indices[i];
  }

  PaddedVertex* ssboData = (PaddedVertex*)&indexData[INDEX_COUNT];
  for (u32 i = 0; i < VERT_COUNT; i++) {
    ssboData[i] = padded_verts[i];
  }

  SDL_UnmapGPUTransferBuffer(Device, trBuf);

  // Upload the transfer data to the GPU resources
  SDL_GPUCommandBuffer* uploadCmdBuf = SDL_AcquireGPUCommandBuffer(Device);
  if (!uploadCmdBuf) {
    LOG_ERROR("couldn't acquire command buffer");
    RELEASE_BUFFERS
    return false;
  }
  SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCmdBuf);

  SDL_GPUTransferBufferLocation trLoc;
  {
    trLoc.transfer_buffer = trBuf;
    trLoc.offset = 0;
  }
  SDL_GPUBufferRegion reg;
  {
    reg.buffer = vertex_buffer_;
    reg.offset = 0;
    reg.size = VBUF_SZ;
  };
  SDL_UploadToGPUBuffer(copyPass, &trLoc, &reg, false);

  {
    trLoc.offset = VBUF_SZ;
    reg.buffer = index_buffer_;
    reg.size = IBUF_SZ;
  }
  SDL_UploadToGPUBuffer(copyPass, &trLoc, &reg, false);

  {
    trLoc.offset = VBUF_SZ + IBUF_SZ;
    reg.buffer = ssbo_;
    reg.size = SSBO_SZ;
  }
  SDL_UploadToGPUBuffer(copyPass, &trLoc, &reg, false);

  SDL_EndGPUCopyPass(copyPass);
  bool ret = SDL_SubmitGPUCommandBuffer(uploadCmdBuf);
  if (!ret) {
    LOG_ERROR("couldn't submit copy pass command buffer");
    RELEASE_BUFFERS
  } else {
    LOG_DEBUG("Uploaded vertex data to GPU");
    SDL_ReleaseGPUTransferBuffer(Device, trBuf);
  }
  return ret;
#undef RELEASE_BUFFERS
}

// bool
// BufferApp::SendVertexData()
// {
//   LOG_TRACE("BufferApp::SendVertexData");
//
//   SDL_GPUBufferCreateInfo vert_info{};
//   {
//     vert_info.usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
//     vert_info.size = sizeof(PosColVertex) * VERT_COUNT;
//   }
//
//   SDL_GPUBufferCreateInfo idx_info{};
//   {
//     idx_info.usage = SDL_GPU_BUFFERUSAGE_INDEX;
//     idx_info.size = sizeof(u16) * INDEX_COUNT;
//   }
//
//   SDL_GPUTransferBufferCreateInfo transferInfo{};
//   {
//     Uint32 sz = sizeof(PosColVertex) * VERT_COUNT + sizeof(u16) *
//     INDEX_COUNT; transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
//     transferInfo.size = sz;
//   }
//
//   ssbo_ = SDL_CreateGPUBuffer(Device, &vert_info);
//   if (!ssbo_) {
//     LOG_ERROR("couldn't create storage buffer");
//     return false;
//   }
//
//   index_buffer_ = SDL_CreateGPUBuffer(Device, &idx_info);
//   if (!index_buffer_) {
//     LOG_ERROR("couldn't create index buffer");
//     SDL_ReleaseGPUBuffer(Device, ssbo_);
//     return false;
//   }
//
//   SDL_GPUTransferBuffer* trBuf =
//     SDL_CreateGPUTransferBuffer(Device, &transferInfo);
//
//   if (!trBuf) {
//     LOG_ERROR("couldn't create transfer buffer");
//     SDL_ReleaseGPUBuffer(Device, ssbo_);
//     SDL_ReleaseGPUBuffer(Device, index_buffer_);
//     return false;
//   }
//
//   PosColVertex* transferData =
//     (PosColVertex*)SDL_MapGPUTransferBuffer(Device, trBuf, false);
//   if (!transferData) {
//     LOG_ERROR("couldn't get mapping for transfer buffer");
//     RELEASE_BUFFERS
//     return false;
//   }
//
//   for (u32 i = 0; i < VERT_COUNT; ++i) {
//     transferData[i] = verts[i];
//   }
//
//   u16* indexData = (u16*)&transferData[VERT_COUNT];
//   for (u32 i = 0; i < INDEX_COUNT; ++i) {
//     indexData[i] = indices[i];
//   }
//
//   SDL_UnmapGPUTransferBuffer(Device, trBuf);
//
//   // Upload the transfer data to the GPU resources
//   SDL_GPUCommandBuffer* uploadCmdBuf = SDL_AcquireGPUCommandBuffer(Device);
//   if (!uploadCmdBuf) {
//     LOG_ERROR("couldn't acquire command buffer");
//     RELEASE_BUFFERS
//     return false;
//   }
//   SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCmdBuf);
//
//   SDL_GPUTransferBufferLocation trLoc;
//   {
//     trLoc.transfer_buffer = trBuf;
//     trLoc.offset = 0;
//   }
//   SDL_GPUBufferRegion reg;
//   {
//     reg.buffer = ssbo_;
//     reg.offset = 0;
//     reg.size = static_cast<u32>(sizeof(PosColVertex) * VERT_COUNT);
//   };
//   SDL_UploadToGPUBuffer(copyPass, &trLoc, &reg, false);
//
//   trLoc.offset = sizeof(PosColVertex) * VERT_COUNT;
//   reg.buffer = index_buffer_;
//   reg.size = sizeof(u16) * INDEX_COUNT;
//
//   SDL_UploadToGPUBuffer(copyPass, &trLoc, &reg, false);
//
//   SDL_EndGPUCopyPass(copyPass);
//   bool ret = SDL_SubmitGPUCommandBuffer(uploadCmdBuf);
//   if (!ret) {
//     LOG_ERROR("couldn't submit copy pass command buffer");
//     RELEASE_BUFFERS
//   } else {
//     LOG_DEBUG("Uploaded vertex data to GPU");
//     SDL_ReleaseGPUTransferBuffer(Device, trBuf);
//   }
//   return ret;
// }
