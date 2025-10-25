#include <pch.h>
#include <vector>

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
SceneNode::Draw(glm::mat4 matrix, RenderContext& context)
{
  for (const auto& child : Children) {
    child->Draw(matrix, context);
  }
}

void
MeshNode::Draw(glm::mat4 matrix, RenderContext& context)
{
  glm::mat4 mat = matrix * WorldMatrix;
  for (const auto& submesh : Mesh->Submeshes) {
    bool isOpaque = (submesh.material->Opacity == MaterialOpacity::Opaque);
    std::vector<RenderItem>& draws =
      isOpaque ? context.OpaqueItems : context.TransparentItems;
    draws.emplace_back(RenderItem{ mat,
                                   Mesh->VertexBuffer(),
                                   Mesh->IndexBuffer(),
                                   submesh.FirstIndex,
                                   submesh.VertexCount,
                                   submesh.material });
  }
  SceneNode::Draw(matrix, context); // recurse down on children
}

// TODO
bool
Renderer::Draw([[maybe_unused]] SharedPtr<SceneNode> scene_root,
               [[maybe_unused]] Camera* camera) const
{
  // TODO
  return true;
}
