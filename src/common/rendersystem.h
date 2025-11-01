#pragma once

#include <vector>

#include "common/camera.h"
#include "common/material.h"

#include <SDL3/SDL_gpu.h>
#include <glm/ext/matrix_float4x4.hpp>

struct RenderItem
{
  glm::mat4x4 matrix;
  SDL_GPUBuffer* VertexBuffer{};
  SDL_GPUBuffer* IndexBuffer{};
  const std::size_t FirstIndex;
  const std::size_t VertexCount;
  SharedPtr<MaterialInstance> Material{ nullptr };
};

struct RenderContext
{
  std::vector<RenderItem> OpaqueItems{};
  std::vector<RenderItem> TransparentItems{};
  void Clear()
  {
    OpaqueItems.clear();
    TransparentItems.clear();
  }
};

struct IRenderable
{
  // push RenderItems to the list
  virtual void Draw(glm::mat4 matrix, RenderContext& context) = 0;
};

struct Geometry
{
  const std::size_t FirstIndex;
  const std::size_t VertexCount;
  std::shared_ptr<MaterialInstance> material{ nullptr };
};

// Vertex + Index buffer combo
struct MeshBuffers
{
  SDL_GPUBuffer* VertexBuffer{};
  SDL_GPUBuffer* IndexBuffer{};
};

struct MeshAsset
{
  const char* Name;
  std::vector<Geometry> Submeshes;

  MeshBuffers Buffers{};
  SDL_GPUBuffer* VertexBuffer() const { return Buffers.VertexBuffer; }
  SDL_GPUBuffer* IndexBuffer() const { return Buffers.IndexBuffer; }
};

struct SceneNode : public IRenderable
{
  WeakPtr<SceneNode> Parent;
  std::vector<SharedPtr<SceneNode>> Children;
  glm::mat4 LocalMatrix;
  glm::mat4 WorldMatrix;
  void Update(glm::mat4 parentMatrix);
  void Draw(glm::mat4 matrix, RenderContext& context) override;
  virtual ~SceneNode() = default;
};

struct MeshNode final : public SceneNode
{
  MeshAsset* Mesh;
  void Draw(glm::mat4 matrix, RenderContext& context) override;
};

class Renderer
{
public:
  explicit Renderer(SDL_GPUDevice* device, SDL_Window* window)
    : device_{ device }
    , window_{ window }
  {
  }
  bool Draw([[maybe_unused]] SharedPtr<SceneNode> scene_root,
            [[maybe_unused]] Camera* camera) const;

private:
  [[maybe_unused]] SDL_GPUDevice* device_;
  [[maybe_unused]] SDL_Window* window_;
};
