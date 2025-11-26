#include "common/logger.h"
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

/* *
 * NOTE: If we just disable blending, alpha values below one can still
 * be written to framebuffer. Blending is enabled to force alpha to always
 * be one to avoid ImGUI's background showing behind framebuffer
 * */
void
disable_blending(SDL_GPUColorTargetDescription& d)
{
  d.blend_state.enable_blend = true;
  d.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
  d.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
  d.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;

  d.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
  d.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
  d.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
}

void
enable_blending(SDL_GPUColorTargetDescription& d)
{
  d.blend_state.enable_blend = true;
  d.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
  d.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
  d.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;

  d.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
  d.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
  d.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
}

u32
vertex_attribute_size(SDL_GPUVertexElementFormat f)
{
  switch (f) {
    case SDL_GPU_VERTEXELEMENTFORMAT_BYTE2:
    case SDL_GPU_VERTEXELEMENTFORMAT_UBYTE2:
    case SDL_GPU_VERTEXELEMENTFORMAT_BYTE2_NORM:
    case SDL_GPU_VERTEXELEMENTFORMAT_UBYTE2_NORM:
      return 2;

    case SDL_GPU_VERTEXELEMENTFORMAT_INT:
    case SDL_GPU_VERTEXELEMENTFORMAT_UINT:
    case SDL_GPU_VERTEXELEMENTFORMAT_FLOAT:
    case SDL_GPU_VERTEXELEMENTFORMAT_SHORT2:
    case SDL_GPU_VERTEXELEMENTFORMAT_USHORT2:
    case SDL_GPU_VERTEXELEMENTFORMAT_SHORT2_NORM:
    case SDL_GPU_VERTEXELEMENTFORMAT_USHORT2_NORM:
    case SDL_GPU_VERTEXELEMENTFORMAT_HALF2:
    case SDL_GPU_VERTEXELEMENTFORMAT_BYTE4:
    case SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4:
    case SDL_GPU_VERTEXELEMENTFORMAT_BYTE4_NORM:
    case SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM:
      return 4;

    case SDL_GPU_VERTEXELEMENTFORMAT_INT2:
    case SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2:
    case SDL_GPU_VERTEXELEMENTFORMAT_UINT2:
    case SDL_GPU_VERTEXELEMENTFORMAT_SHORT4:
    case SDL_GPU_VERTEXELEMENTFORMAT_USHORT4:
    case SDL_GPU_VERTEXELEMENTFORMAT_SHORT4_NORM:
    case SDL_GPU_VERTEXELEMENTFORMAT_USHORT4_NORM:
    case SDL_GPU_VERTEXELEMENTFORMAT_HALF4:
      return 8;

    case SDL_GPU_VERTEXELEMENTFORMAT_INT3:
    case SDL_GPU_VERTEXELEMENTFORMAT_UINT3:
    case SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3:
      return 12;

    case SDL_GPU_VERTEXELEMENTFORMAT_INT4:
    case SDL_GPU_VERTEXELEMENTFORMAT_UINT4:
    case SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4:
      return 16;

    case SDL_GPU_VERTEXELEMENTFORMAT_INVALID:
    default:
      LOG_WARN("Unknown Vertex Attribute format");
      return 0;
      return 0;
  }
}
