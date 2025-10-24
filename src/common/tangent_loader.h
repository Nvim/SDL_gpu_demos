#pragma once

#include <vector>

#include "common/util.h"

#include <mikktspace/mikktspace.h>

struct CPUMeshBuffers
{
  std::vector<PosNormalTangentColorUvVertex>* VertexBuffer;
  std::vector<u32>* IndexBuffer;
};

class TangentLoader
{
public:
  virtual void Load(CPUMeshBuffers* buffers) = 0;
  virtual ~TangentLoader() = default;
};
/* *
 * This class uses the method described in OGLDev's [tutorial
 * #26](https://ogldev.org/www/tutorial26/tutorial26.html) to pre-compute the
 * tangents. Unlike mikktspace, it doesn't break indices, so the results will
 * look better. The result isn't perfect however, and using gltf-transform
 * will provide more satisfying results.
 * */
class OGLDevTangentLoader final : public TangentLoader
{
public:
  OGLDevTangentLoader() = default;
  ~OGLDevTangentLoader() = default;
  void Load(CPUMeshBuffers* buffers) override;
};

/* *
 * NOTE:
 * This class implements the interface for
 * [mikktspace](http://www.mikktspace.com/). However, it's explicitly stated
 * that tangents are calculated per vertex and indices shouldn't be re-used.
 * This implementation re-uses indices and will yield incorrect results. A
 * proper fix (which I know I'll never do) would be to unweld the vertices
 * first, and reweld them afterwards, like
 * [gltf-transform](https://github.com/donmccurdy/glTF-Transform) does.
 * */

class MikktspaceTangentLoader final : public TangentLoader
{
public:
  MikktspaceTangentLoader();
  ~MikktspaceTangentLoader() = default;
  void Load(CPUMeshBuffers* buffers) override;

public:
  // CPUMeshBuffers Buffers{};
  SMikkTSpaceContext Context{};
  SMikkTSpaceInterface Iface{};

private:
  // impl iface:
  static i32 get_num_faces(const SMikkTSpaceContext* pContext);
  static i32 get_num_vertices_of_face(const SMikkTSpaceContext* pContext,
                                      const int iFace);
  static void get_position(const SMikkTSpaceContext* pContext,
                           float fvPosOut[],
                           const int iFace,
                           const int iVert);
  static void get_normal(const SMikkTSpaceContext* pContext,
                         float fvPosOut[],
                         const int iFace,
                         const int iVert);
  static void get_tex_uv(const SMikkTSpaceContext* pContext,
                         float fvPosOut[],
                         const int iFace,
                         const int iVert);
  static void set_tspace(const SMikkTSpaceContext* pContext,
                         const float fvTangent[],
                         const float fSign,
                         const int iFace,
                         const int iVert);

  // util:

  static constexpr CPUMeshBuffers* GetBuffers(
    const SMikkTSpaceContext* pContext)
  {
    return static_cast<CPUMeshBuffers*>(pContext->m_pUserData);
  }

  static u32 get_idx(const SMikkTSpaceContext* context, int iFace, int iVert);
};
