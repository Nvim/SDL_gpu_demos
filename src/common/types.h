#pragma once

#include <SDL3/SDL_gpu.h>
#include <memory>

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
using f32 = float;
using f64 = double;

template<typename T> //
using UniquePtr = std::unique_ptr<T>;

template<typename T, typename... U>
UniquePtr<T>
MakeUnique(U... args)
{
  return std::make_unique<T>(args...);
}

template<typename T> //
using SharedPtr = std::shared_ptr<T>;

template<typename T, typename... U>
SharedPtr<T>
MakeShared(U... args)
{
  return std::make_shared<T>(args...);
}

template<typename T>
using WeakPtr = std::weak_ptr<T>;

static constexpr size_t FLOAT4 = sizeof(glm::vec4);
static constexpr size_t FLOAT3 = sizeof(glm::vec3);
static constexpr size_t FLOAT2 = sizeof(glm::vec2);
static_assert(FLOAT4 == 16);
static_assert(FLOAT3 == 12);
static_assert(FLOAT2 == 8);

struct PosVertex
{
  float pos[3];
};

struct PosVertex_Aligned
{
  glm::vec3 pos;
  float pad_;
};

struct PosNormalVertex_Aligned
{
  glm::vec3 pos;
  float pad_0;
  glm::vec3 normal;
  float pad_1;
};

struct PosColVertex
{
  float poscol[6];
};

struct PosUvVertex
{
  float pos[3];
  float uv[2];
};

struct PosNormalUvVertex
{
  glm::vec3 pos;
  glm::vec3 normal{ 0.f };
  glm::vec2 uv{ 0.f };
};

struct PosNormalColorUvVertex
{
  glm::vec3 pos;
  glm::vec3 normal{ 0.f };
  glm::vec2 uv{ 0.f };
  glm::vec4 color{ 1.f };
};

struct PosNormalTangentColorUvVertex
{
  glm::vec3 pos;
  glm::vec3 normal{ 0.f };
  glm::vec4 tangent{ 0.f };
  glm::vec2 uv{ 0.f };
  glm::vec4 color{ 1.f };
};

struct Flag
{
  const char* label;
  u32 flag_value;
};

struct CameraBinding
{
  glm::mat4 viewproj;
  glm::mat4 camera_model;
  glm::vec4 camera_world;
};

struct DirLightBinding
{
  glm::vec3 direction = { 10.f, 10.f, 10.f };
  f32 pad_0;
  glm::vec3 ambient = { .2f, .2f, .2f };
  f32 pad_1;
  glm::vec3 diffuse = { .8f, .8f, .8f };
  f32 pad_2;
  glm::vec3 specular = { .8f, .8f, .8f };
};

class TransferBufferWrapper
{
private:
  SDL_GPUDevice* device_;
  SDL_GPUTransferBuffer* buffer_;

public:
  SDL_GPUTransferBuffer* Get() const { return buffer_; }

public:
  // Movable, although i see no reason to
  TransferBufferWrapper(const TransferBufferWrapper&) = delete;
  TransferBufferWrapper& operator=(const TransferBufferWrapper&) = delete;

  // TODO: handle download someday
  explicit TransferBufferWrapper(SDL_GPUDevice* device, u32 size)
    : device_{ device }
  {
    SDL_GPUTransferBufferCreateInfo info{};
    info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    info.size = size;
    buffer_ = SDL_CreateGPUTransferBuffer(device_, &info);
  }

  ~TransferBufferWrapper()
  {
    if (buffer_) {
      SDL_ReleaseGPUTransferBuffer(device_, buffer_);
    }
  }

  TransferBufferWrapper(TransferBufferWrapper&& other) noexcept
    : device_{ other.device_ }
    , buffer_{ other.buffer_ }
  {
    other.buffer_ = nullptr;
  }

  TransferBufferWrapper& operator=(TransferBufferWrapper&& other) noexcept
  {
    if (this == &other) {
      return *this;
    }
    if (this->buffer_) {
      SDL_ReleaseGPUTransferBuffer(device_, this->buffer_);
    }
    device_ = other.device_;
    buffer_ = other.buffer_;
    other.buffer_ = nullptr;

    return *this;
  }
};
