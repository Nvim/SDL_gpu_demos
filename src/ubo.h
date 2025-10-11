#pragma once

#include "src/logger.h"
#include "src/types.h"
#include <SDL3/SDL_gpu.h>
#include <cassert>
#include <glm/ext/vector_float4.hpp>
// #include <type_traits>

enum class UBOBindingType
{
  Vertex,
  Fragment,
};

// UBO on Fragment shader side
struct MaterialDataBinding
{
  glm::vec4 color_factors;
  glm::vec4 metal_rough_factors;
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
