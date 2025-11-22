#pragma once

#include <vector>

#include "common/gltf_material.h"
#include "common/gltf_scene.h"
#include "common/loaded_image.h"
#include "common/rendersystem.h"
#include "common/tangent_loader.h"
#include "common/types.h"

#include <SDL3/SDL_gpu.h>
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <glm/ext/matrix_float4x4.hpp>

class Engine;

class GLTFLoader
{
  friend class GLTFScene;

public:
  GLTFLoader(Engine* engine);
  GLTFLoader(Engine* engine, SDL_GPUTextureFormat framebuffer_format);
  ~GLTFLoader();

  bool Load(GLTFScene* scene, std::filesystem::path& path);
  UniquePtr<GLTFScene> Load(std::filesystem::path& path);
  void Release(); // Callable dtor, must be destroyed before app

public:
  static constexpr const char* VertexShaderPath =
    "resources/shaders/compiled/vert.spv";
  static constexpr const char* FragmentShaderPath =
    "resources/shaders/compiled/frag.spv";

private:
  bool LoadVertexData(GLTFScene* ret);
  bool LoadSamplers(GLTFScene* ret);
  bool LoadImageData(GLTFScene* ret);
  bool LoadTexture(GLTFScene* ret, u64 texture_index, bool srgb);
  bool LoadMaterials(GLTFScene* ret);
  bool LoadNodes(GLTFScene* ret);

  bool Parse(std::filesystem::path& path);
  bool LoadResources(GLTFScene* ret);

  // TODO: some of these utils should be moved somewhere else
  template<typename V, typename I>
  bool UploadBuffers(MeshBuffers* buffers,
                     std::vector<V> vertices,
                     std::vector<I> indices);

  void LoadImageFromURI(LoadedImage& img,
                        std::filesystem::path parent_path,
                        const fastgltf::sources::URI& URI);
  void LoadImageFromVector(LoadedImage& img,
                           const fastgltf::sources::Vector& vector);
  void LoadImageFromBufferView(LoadedImage& img,
                               const fastgltf::BufferView& view,
                               const fastgltf::Buffer& buffer);
  SDL_GPUTexture* CreateAndUploadTexture(LoadedImage& img, bool srgb);
  bool CreateDefaultTexture();
  bool CreateDefaultSampler();
  void CreateDefaultMaterial();
  bool CreatePipelines();
  bool IsInitialized();

private:
  Engine* engine_;

  // Used for material pipeline creation, default assumes HDR framebuffer
  SDL_GPUTextureFormat framebuffer_format_ =
    SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
  fastgltf::Asset asset_;
  UniquePtr<TangentLoader> tangent_loader_{ nullptr };

  SDL_GPUSampler* default_sampler_{ nullptr };
  SDL_GPUTexture* default_texture_{ nullptr };
  SharedPtr<GLTFPbrMaterial> default_material_{ nullptr };

  SDL_GPUGraphicsPipeline* opaque_pipeline_{ nullptr };
  SDL_GPUGraphicsPipeline* transparent_pipeline_{ nullptr };
};
