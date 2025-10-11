#include "gltf_scene.h"
#include "gltf_loader.h"

GLTFScene::GLTFScene(std::filesystem::path path, const GLTFLoader* loader)
  : Path{ path }
  , loader_{ loader }
{
}

GLTFScene::~GLTFScene()
{
  if (loaded_) {
    Release();
  }
}

void
GLTFScene::Release()
{
  LOG_TRACE("Destroying GLTFScene");
  {
    auto Device = loader_->program_->Device;
    for (auto* tex : textures_) {
      if (tex != loader_->default_texture_) {
        RELEASE_IF(tex, SDL_ReleaseGPUTexture);
      }
    }
    for (auto* sampler : samplers_) {
      if (sampler != loader_->default_sampler_) {
        RELEASE_IF(sampler, SDL_ReleaseGPUSampler);
      }
    }
    for (auto& mesh : meshes_) {
      RELEASE_IF(mesh.IndexBuffer(), SDL_ReleaseGPUBuffer);
      RELEASE_IF(mesh.VertexBuffer(), SDL_ReleaseGPUBuffer);
    }
    LOG_DEBUG("Released GLTF resources");
  }
  loaded_ = false;
}

void
GLTFScene::Draw(glm::mat4 matrix, std::vector<RenderItem>& draws)
{
  for (const auto& parent : parent_nodes_) {
    parent->Draw(matrix, draws);
  }
}

const std::vector<MeshAsset>&
GLTFScene::Meshes() const
{
  return meshes_;
}

const std::vector<SDL_GPUTexture*>&
GLTFScene::Textures() const
{
  return textures_;
}

const std::vector<SDL_GPUSampler*>&
GLTFScene::Samplers() const
{
  return samplers_;
}
const std::vector<SharedPtr<GLTFPbrMaterial>>&
GLTFScene::Materials() const
{
  return materials_;
}

const std::vector<SharedPtr<SceneNode>>&
GLTFScene::AllNodes() const
{
  return all_nodes_;
}

const std::vector<SharedPtr<SceneNode>>&
GLTFScene::ParentNodes() const
{
  return parent_nodes_;
}
