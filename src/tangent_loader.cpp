#include "tangent_loader.h"
#include "mikktspace/mikktspace.h"
#include "src/logger.h"
#include <assert.h>
#include <glm/geometric.hpp>

MikktspaceTangentLoader::MikktspaceTangentLoader()
{
  Iface.m_getNumFaces = get_num_faces;
  Iface.m_getNumVerticesOfFace = get_num_vertices_of_face;
  Iface.m_getPosition = get_position;
  Iface.m_getNormal = get_normal;
  Iface.m_getTexCoord = get_tex_uv;
  Iface.m_setTSpaceBasic = set_tspace;

  Context.m_pInterface = &Iface;
}

void
MikktspaceTangentLoader::Load(CPUMeshBuffers* buffers)
{
  LOG_TRACE("MikktspaceTangentLoader::Load");
  Context.m_pUserData = buffers;
  genTangSpaceDefault(&Context);
}

i32
MikktspaceTangentLoader::get_num_faces(const SMikkTSpaceContext* pContext)
{
  auto buffers = GetBuffers(pContext);
  auto sz = buffers->IndexBuffer->size();
  assert(sz != 0);
  return sz / 3; // TODO: quad primitive support
}

i32
MikktspaceTangentLoader::get_num_vertices_of_face(
  [[maybe_unused]] const SMikkTSpaceContext* pContext,
  [[maybe_unused]] const int iFace)
{
  return 3; // TODO: quad primitive support
}

void
MikktspaceTangentLoader::get_position(const SMikkTSpaceContext* pContext,
                                      float fvPosOut[],
                                      const int iFace,
                                      const int iVert)
{
  auto buffers = GetBuffers(pContext);
  auto* verts = buffers->VertexBuffer;
  auto idx = get_idx(pContext, iFace, iVert);
  auto& vert = verts->at(idx);

  fvPosOut[0] = vert.pos.x;
  fvPosOut[1] = vert.pos.y;
  fvPosOut[2] = vert.pos.z;
}

void
MikktspaceTangentLoader::get_normal(const SMikkTSpaceContext* pContext,
                                    float fvPosOut[],
                                    const int iFace,
                                    const int iVert)
{
  auto buffers = GetBuffers(pContext);
  auto* verts = buffers->VertexBuffer;
  auto idx = get_idx(pContext, iFace, iVert);
  auto& vert = verts->at(idx);

  fvPosOut[0] = vert.normal.x;
  fvPosOut[1] = vert.normal.y;
  fvPosOut[2] = vert.normal.z;
}
void
MikktspaceTangentLoader::get_tex_uv(const SMikkTSpaceContext* pContext,
                                    float fvPosOut[],
                                    const int iFace,
                                    const int iVert)
{
  auto buffers = GetBuffers(pContext);
  auto* verts = buffers->VertexBuffer;
  auto idx = get_idx(pContext, iFace, iVert);
  auto& vert = verts->at(idx);

  fvPosOut[0] = vert.uv.x;
  fvPosOut[1] = vert.uv.y;
}

void
MikktspaceTangentLoader::set_tspace(const SMikkTSpaceContext* pContext,
                                    const float fvTangent[],
                                    const float fSign,
                                    const int iFace,
                                    const int iVert)
{
  auto buffers = GetBuffers(pContext);
  auto* verts = buffers->VertexBuffer;
  auto idx = get_idx(pContext, iFace, iVert);
  auto& vert = verts->at(idx);

  vert.tangent.x = fvTangent[0];
  vert.tangent.y = fvTangent[1];
  vert.tangent.z = fvTangent[2];
  vert.tangent.w = fSign;
  assert(vert.tangent.w == 1.f || vert.tangent.w == -1.f);
}

u32
MikktspaceTangentLoader::get_idx(const SMikkTSpaceContext* context,
                                 int iFace,
                                 int iVert)
{
  auto buffers = GetBuffers(context);
  std::vector<u32>* idxs = buffers->IndexBuffer;

  auto face_size = get_num_vertices_of_face(context, iFace);
  auto indices_index = (iFace * face_size) + iVert;
  auto idx = idxs->at(indices_index);
  return idx;
}

// OGLDEV
void
OGLDevTangentLoader::Load(CPUMeshBuffers* buffers)
{
  LOG_TRACE("OGLDevTangentLoader::Load");
  auto& vertices = *buffers->VertexBuffer;
  auto& indices = *buffers->IndexBuffer;
  assert(indices.size() != 0);
  assert(indices.size() % 3 == 0);

  for (unsigned int i = 0; i < indices.size(); i += 3) {
    auto& v0 = vertices[indices[i]];
    auto& v1 = vertices[indices[i + 1]];
    auto& v2 = vertices[indices[i + 2]];

    glm::vec3 edge1 = v1.pos - v0.pos;
    glm::vec3 edge2 = v2.pos - v0.pos;

    float deltaU1 = v1.uv.x - v0.uv.x;
    float deltaV1 = v1.uv.y - v0.uv.y;
    float deltaU2 = v2.uv.x - v0.uv.x;
    float deltaV2 = v2.uv.y - v0.uv.y;

    float f = 1.0f / (deltaU1 * deltaV2 - deltaU2 * deltaV1);

    glm::vec4 tangent;
    tangent.x = f * (deltaV2 * edge1.x - deltaV1 * edge2.x);
    tangent.y = f * (deltaV2 * edge1.y - deltaV1 * edge2.y);
    tangent.z = f * (deltaV2 * edge1.z - deltaV1 * edge2.z);
    tangent.w = 1.f; // TODO: figure out when this should be -1 ?

    v0.tangent += tangent;
    v1.tangent += tangent;
    v2.tangent += tangent;
  }

  for (unsigned int i = 0; i < vertices.size(); i++) {
    vertices[i].tangent = glm::normalize(vertices[i].tangent);
  }
}
