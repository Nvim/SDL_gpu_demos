#include "pch.h"
#include "rendersystem.h"

void
SceneNode::Update(glm::mat4 parentMatrix)
{
  WorldMatrix = LocalMatrix * parentMatrix;
  for (const auto& child : Children) {
    child->Update(WorldMatrix);
  }
}

void
SceneNode::Draw(glm::mat4 matrix, std::vector<RenderItem>& draws)
{
  for (const auto& child : Children) {
    child->Draw(matrix, draws);
  }
}

void
MeshNode::Draw(glm::mat4 matrix, std::vector<RenderItem>& draws)
{
  glm::mat4 mat = matrix * WorldMatrix;
  for (const auto& submesh : Mesh->Submeshes) {
    draws.emplace_back(RenderItem{ mat,
                                   Mesh->VertexBuffer(),
                                   Mesh->IndexBuffer(),
                                   submesh.FirstIndex,
                                   submesh.VertexCount,
                                   submesh.material });
  }
  SceneNode::Draw(matrix, draws); // recurse down on children
}

// TODO
bool
Renderer::Draw([[maybe_unused]] SharedPtr<SceneNode> scene_root,
               [[maybe_unused]] Camera* camera) const
{
  // TODO
  return true;
}
