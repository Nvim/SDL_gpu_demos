#pragma once

#include <cassert>

#include "common/logger.h"
#include "common/types.h"

#include <SDL3/SDL_gpu.h>
#include <glm/ext/vector_float4.hpp>

enum class UBOBindingType
{
  Vertex,
  Fragment,
};

// UBO on Fragment shader side
struct MaterialDataBinding
{
  glm::vec4 color_factors; // 0-15
  f32 metal_factor;        // 16-19
  f32 rough_factor;        // 20-23
  u32 feature_flags;       // 23-27
  f32 __pad[1];            // 27-31
};

template<typename T>
struct UBO
{
  UBOBindingType type = UBOBindingType::Vertex;
  u32 slot = 1;
  T data{};
  u64 size = sizeof(std::remove_reference_t<T>);

  UBO() = default;
  UBO(UBOBindingType type, u32 slot, T data)
    : type{ type }
    , slot{ slot }
    , data{ std::move(data) }
  {
    static_assert(sizeof(data) == sizeof(T));
    static_assert(sizeof(data) == sizeof(T));
    size = sizeof(data);
  }

  void Bind(SDL_GPUCommandBuffer* cmdbuf) const
  {
    switch (type) {
      case UBOBindingType::Fragment:
        SDL_PushGPUFragmentUniformData(cmdbuf, slot, &data, size);
        break;
      case UBOBindingType::Vertex:
        SDL_PushGPUVertexUniformData(cmdbuf, slot, &data, size);
        break;
      default:
        LOG_ERROR("Unsupported binding type");
    }
  }
};
