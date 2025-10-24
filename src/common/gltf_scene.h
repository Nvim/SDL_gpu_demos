#pragma once

#include <filesystem>

#include "common/gltf_material.h"
#include "common/rendersystem.h"

#include <SDL3/SDL_gpu.h>

class GLTFLoader;

// Represents a loaded GLTF model
class GLTFScene final : public IRenderable
{
  friend class GLTFLoader;

public:
  GLTFScene() = default;
  GLTFScene(std::filesystem::path path, const GLTFLoader* loader);
  ~GLTFScene();

  void Draw(glm::mat4 matrix, std::vector<RenderItem>& draws) override;
  void Release();

  const std::vector<MeshAsset>& Meshes() const;
  const std::vector<SDL_GPUTexture*>& Textures() const;
  const std::vector<SDL_GPUSampler*>& Samplers() const;
  const std::vector<SharedPtr<GLTFPbrMaterial>>& Materials() const;
  const std::vector<SharedPtr<SceneNode>>& AllNodes() const;
  const std::vector<SharedPtr<SceneNode>>& ParentNodes() const;

public:
  std::filesystem::path Path;

private:
  bool loaded_{ false };
  const GLTFLoader* loader_;
  std::vector<MeshAsset> meshes_;
  std::vector<SDL_GPUTexture*> textures_;
  std::vector<SDL_GPUSampler*> samplers_;
  std::vector<SharedPtr<GLTFPbrMaterial>> materials_;
  std::vector<std::shared_ptr<SceneNode>> parent_nodes_;
  std::vector<std::shared_ptr<SceneNode>> all_nodes_;
};
