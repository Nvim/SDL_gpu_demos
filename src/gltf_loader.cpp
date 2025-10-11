#include "gltf_loader.h"
#include "fastgltf/math.hpp"
#include "logger.h"
#include "src/cube.h"
#include "src/material.h"
#include "src/rendersystem.h"
#include "src/types.h"
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_gpu.h>
#define GLM_ENABLE_EXPERIMENTAL 1
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/quaternion_common.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/gtx/quaternion.hpp>
#include <memory>
#include <vector>

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#endif
#include "fastgltf/glm_element_traits.hpp"
#include "fastgltf/types.hpp"
#include "src/util.h"
#include <SDL3/SDL_surface.h>
#include <cassert>
#include <fastgltf/tools.hpp>
#include <filesystem>
#include <glm/ext/vector_float3.hpp>
#include <stb/stb_image.h>
#include <string>
#include <variant>

namespace {
SDL_GPUFilter
extract_filter(fastgltf::Filter filter)
{
  switch (filter) {
    // nearest samplers
    case fastgltf::Filter::Nearest:
    case fastgltf::Filter::NearestMipMapNearest:
    case fastgltf::Filter::NearestMipMapLinear:
      return SDL_GPU_FILTER_NEAREST;

    // linear samplers
    case fastgltf::Filter::Linear:
    case fastgltf::Filter::LinearMipMapNearest:
    case fastgltf::Filter::LinearMipMapLinear:
    default:
      return SDL_GPU_FILTER_LINEAR;
  }
}

SDL_GPUSamplerMipmapMode
extract_mipmap_mode(fastgltf::Filter filter)
{
  switch (filter) {
    case fastgltf::Filter::NearestMipMapNearest:
    case fastgltf::Filter::LinearMipMapNearest:
      return SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;

    case fastgltf::Filter::NearestMipMapLinear:
    case fastgltf::Filter::LinearMipMapLinear:
    default:
      return SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
  }
}

}

GLTFLoader::GLTFLoader(Program* program, std::filesystem::path path)
  : program_{ program }
  , path_{ path }
{
}

GLTFLoader::~GLTFLoader()
{
  LOG_TRACE("Destroying GLTFLoader");
}

void
GLTFLoader::Release()
{
  LOG_TRACE("GLTFLoader::Release");
  auto Device = program_->Device;
  for (auto& tex : textures_) {
    if (tex != default_texture_) {
      RELEASE_IF(tex, SDL_ReleaseGPUTexture);
    }
  }
  for (auto& sampler : samplers_) {
    if (sampler != default_sampler_) {
      RELEASE_IF(sampler, SDL_ReleaseGPUSampler);
    }
  }
  for (auto& mesh : meshes_) {
    RELEASE_IF(mesh.IndexBuffer(), SDL_ReleaseGPUBuffer);
    RELEASE_IF(mesh.VertexBuffer(), SDL_ReleaseGPUBuffer);
  }
  RELEASE_IF(default_texture_, SDL_ReleaseGPUTexture);
  RELEASE_IF(default_sampler_, SDL_ReleaseGPUSampler);
  LOG_DEBUG("Released GLTF resources");
}

const std::vector<MeshAsset>&
GLTFLoader::Meshes() const
{
  return meshes_;
}

const std::vector<SDL_GPUTexture*>&
GLTFLoader::Textures() const
{
  return textures_;
}

const std::vector<SDL_GPUSampler*>&
GLTFLoader::Samplers() const
{
  return samplers_;
}
const std::vector<SharedPtr<GLTFPbrMaterial>>&
GLTFLoader::Materials() const
{
  return materials_;
}

bool
GLTFLoader::Load()
{
  LOG_TRACE("GLTFLoader::Load");
  if (!std::filesystem::exists(path_)) {
    LOG_ERROR("path {} is invalid", path_.c_str());
    return false;
  }

  if (!CreateDefaultSampler()) {
    LOG_ERROR("Couldn't create default sampler");
    return false;
  }
  if (!CreateDefaultTexture()) {
    LOG_ERROR("Couldn't create default texture");
    return false;
  }

  auto data = fastgltf::GltfDataBuffer::FromPath(path_);
  if (data.error() != fastgltf::Error::None) {
    LOG_ERROR("couldn't load gltf from path");
    return false;
  }

  constexpr auto gltfOptions = fastgltf::Options::LoadExternalBuffers;

  fastgltf::Asset gltf;
  fastgltf::Parser parser{};

  auto asset = parser.loadGltf(data.get(), path_.parent_path(), gltfOptions);
  // auto asset =
  //   parser.loadGltfBinary(data.get(), path_.parent_path(), gltfOptions);
  if (auto error = asset.error(); error != fastgltf::Error::None) {
    LOG_ERROR("couldn't parse binary gltf");
    return false;
  }

  if (auto error = fastgltf::validate(asset.get());
      error != fastgltf::Error::None) {
    LOG_ERROR("couldn't validate gltf");
    return false;
  }

  asset_ = std::move(asset.get());

  loaded_ = LoadSamplers();
  if (!loaded_) {
    LOG_ERROR("Couldn't load samplers from GLTF");
    return false;
  }
  LOG_DEBUG("Loaded {} GLTF samplers", samplers_.size());
  assert(samplers_.size() != 0);

  loaded_ = LoadImageData();
  if (!loaded_) {
    LOG_ERROR("Couldn't load images from GLTF");
    return false;
  }
  LOG_DEBUG("Loaded GLTF images");

  loaded_ = LoadMaterials();
  if (!loaded_) {
    LOG_ERROR("Couldn't load materials from GLTF");
    return false;
  }
  LOG_DEBUG("Loaded GLTF materials");

  loaded_ = LoadVertexData();
  if (!loaded_) {
    LOG_ERROR("Couldn't load vertex data from GLTF");
    return false;
  }
  LOG_DEBUG("Loaded GLTF meshes");

  loaded_ = LoadNodes();
  if (!loaded_) {
    LOG_ERROR("Couldn't load nodes from GLTF");
    return false;
  }
  LOG_DEBUG("Loaded GLTF Nodes");

  return loaded_;
}

bool
GLTFLoader::LoadVertexData()
{
  LOG_TRACE("GLTFLoader::LoadVertexData");
  if (asset_.meshes.empty()) {
    LOG_WARN("LoadVertexData: GLTF has no meshes");
    return false;
  }
  // construct our Mesh vector at the same time we fill vertices and indices
  // buffers
  meshes_ = std::vector<MeshAsset>{};
  meshes_.reserve(asset_.meshes.size());
  std::vector<PosNormalUvVertex> vertices;
  std::vector<u32> indices;

  for (auto& mesh : asset_.meshes) {
    MeshAsset newMesh;
    newMesh.Name = mesh.name.c_str();
    vertices.clear();
    indices.clear();

    for (auto&& p : mesh.primitives) {
      Geometry newGeometry{
        .FirstIndex = indices.size(),
        .VertexCount = (u32)asset_.accessors[p.indicesAccessor.value()].count
      };

      size_t initial_vtx = vertices.size();

      { // load indexes
        fastgltf::Accessor& indexaccessor =
          asset_.accessors[p.indicesAccessor.value()];
        indices.reserve(indices.size() + indexaccessor.count);

        fastgltf::iterateAccessor<std::uint32_t>(
          asset_, indexaccessor, [&](std::uint32_t idx) {
            indices.push_back(idx + initial_vtx);
          });
      }

      { // load vertex positions
        fastgltf::Accessor& posAccessor =
          asset_.accessors[p.findAttribute("POSITION")->accessorIndex];
        vertices.resize(vertices.size() + posAccessor.count);

        fastgltf::iterateAccessorWithIndex<glm::vec3>(
          asset_, posAccessor, [&](glm::vec3 v, size_t index) {
            PosNormalUvVertex newvtx;
            newvtx.pos[0] = v.x;
            newvtx.pos[1] = v.y;
            newvtx.pos[2] = v.z;
            vertices[initial_vtx + index] = newvtx;
          });
      }

      { // load vertex UVs
        auto attr = p.findAttribute("TEXCOORD_0");
        if (attr != p.attributes.end()) {
          fastgltf::iterateAccessorWithIndex<glm::vec2>(
            asset_,
            asset_.accessors[attr->accessorIndex],
            [&](glm::vec2 v, size_t index) {
              vertices[initial_vtx + index].uv[0] = v.x;
              vertices[initial_vtx + index].uv[1] = v.y;
              // LOG_TRACE("{},{}", v.x, v.y);
            });
        }
      }

      { // load normals:
        auto attr = p.findAttribute("NORMAL");
        if (attr != p.attributes.end()) {
          fastgltf::iterateAccessorWithIndex<glm::vec3>(
            asset_,
            asset_.accessors[attr->accessorIndex],
            [&](glm::vec3 v, size_t index) {
              vertices[initial_vtx + index].normal[0] = v.x;
              vertices[initial_vtx + index].normal[1] = v.y;
              vertices[initial_vtx + index].normal[2] = v.z;
            });
        }
      }

      { // material:
        if (p.materialIndex.has_value()) {
          newGeometry.material = materials_[p.materialIndex.value()]->Build();
        } else {
          newGeometry.material = materials_[0]->Build();
        }
        newGeometry.material->Pipeline =
          static_cast<CubeProgram*>(program_)->ScenePipeline;
      }

      newMesh.Submeshes.push_back(newGeometry);
      LOG_DEBUG("New geometry. Total Verts: {}, Total Indices: {}",
                vertices.size(),
                indices.size());
    }
    if (!UploadBuffers(&newMesh.Buffers, vertices, indices)) {
      LOG_ERROR("Couldn't upload mesh data");
      return false;
    }
    meshes_.emplace_back(newMesh);
  }
  return true;
}

bool
GLTFLoader::LoadImageData()
{
  LOG_TRACE("GLTFLoader::LoadImageData");

  textures_ = std::vector<SDL_GPUTexture*>{};
  textures_.reserve(asset_.textures.size());

  if (asset_.images.empty()) {
    LOG_WARN("LoadImageData: GLTF has no images");
    textures_.push_back(default_texture_);
    return true;
  }

  for (auto& image : asset_.images) {
    LoadedImage imgData{};
    // clang-format off
    std::visit(fastgltf::visitor{ 
      []([[maybe_unused]] auto& arg) {LOG_WARN("No URI source");},
      [&](fastgltf::sources::URI& filePath) {
        LOG_DEBUG("Loading image from URI");
        LoadImageFromURI(imgData, filePath);
      },
      [&](fastgltf::sources::Vector& vec) {
        LOG_DEBUG("Loading image from Vector");
        LoadImageFromVector(imgData,vec);
      },
      [&](fastgltf::sources::BufferView& view) {
        const auto& bufferView = asset_.bufferViews[view.bufferViewIndex];
        const auto& buffer = asset_.buffers[bufferView.bufferIndex];

        LoadImageFromBufferView(imgData, bufferView, buffer);
      }
      }, image.data);
    // clang-format on

    SDL_GPUTexture* tex{ nullptr };
    if (imgData.data) {
      LOG_DEBUG("Creating texture");
      tex = CreateAndUploadTexture(imgData);
      if (!tex) {
        LOG_WARN("Falling back to default textue");
        tex = default_texture_;
      }
      stbi_image_free(imgData.data);
    } else {
      LOG_WARN("Falling back to default textue");
      tex = default_texture_;
    }
    textures_.push_back(tex);
  }

  for(const auto& tex : asset_.textures) {
    if(!tex.imageIndex.has_value()) {
      LOG_WARN("Texture doesn't have image.");
      if (tex.basisuImageIndex.has_value()) {
        LOG_WARN("It has a baseiu");
      }
      if (tex.ddsImageIndex.has_value()) {
        LOG_WARN("It has a dds");
      }
      if (tex.webpImageIndex.has_value()) {
        LOG_WARN("It has a webp");
      }
    }
  }

  LOG_DEBUG("{} textures were created", textures_.size());
  assert(textures_.size() == asset_.images.size());
  return !textures_.empty();
}

void
GLTFLoader::LoadImageFromURI(LoadedImage& img,
                             const fastgltf::sources::URI& URI)
{
  LOG_TRACE("GLTFLoader::LoadImageFromURI");
  assert(URI.fileByteOffset == 0);
  assert(URI.uri.isLocalPath());

  const auto p = URI.uri.path();
  const std::string localPath(p.begin(), p.end());

  // NOTE: Setting req_comp to 4 will force padding of missing alpha in case of
  // 3 channels. Forced it to 4 because there's no SDL equivalent to
  // VK_FORMAT_R8G8B8_UNORM as support is meh accross drivers it seems.
  img.data = stbi_load(
    std::format("{}/{}", path_.parent_path().c_str(), localPath).c_str(),
    &img.w,
    &img.h,
    &img.nrChannels,
    4);
}

void
GLTFLoader::LoadImageFromVector(LoadedImage& img,
                                const fastgltf::sources::Vector& vector)
{
  LOG_TRACE("GLTFLoader::LoadImageFromVector");

  img.data = stbi_load_from_memory((stbi_uc*)vector.bytes.data(),
                                   static_cast<int>(vector.bytes.size()),
                                   &img.w,
                                   &img.h,
                                   &img.nrChannels,
                                   4);
}

void
GLTFLoader::LoadImageFromBufferView(LoadedImage& img,
                                    const fastgltf::BufferView& view,
                                    const fastgltf::Buffer& buffer)
{
  LOG_TRACE("GLTFLoader::LoadImageFromBufferView");

  if (!std::holds_alternative<fastgltf::sources::Array>(buffer.data)) {
    LOG_ERROR("Couldn't load image from BufferView: buffer isn't a Vector");
    return;
  }

  auto& vector = std::get<fastgltf::sources::Array>(buffer.data);
  img.data =
    stbi_load_from_memory((stbi_uc*)(vector.bytes.data() + view.byteOffset),
                          static_cast<int>(view.byteLength),
                          &img.w,
                          &img.h,
                          &img.nrChannels,
                          4);
  if (img.data == nullptr) {
    LOG_ERROR("stb failed");
  }
}

// NOTE: 4 channels hardcoded here to match R8G8B8A8_UNORM format
SDL_GPUTexture*
GLTFLoader::CreateAndUploadTexture(LoadedImage& img)
{
  LOG_TRACE("GLTFLoader::CreateAndUploadTexture");
  auto device = program_->Device;

  // TODO: sRGB for baseColor/emissive
  auto format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  SDL_GPUTextureCreateInfo tex_info{};
  {
    tex_info.type = SDL_GPU_TEXTURETYPE_2D;
    tex_info.format = format;
    tex_info.width = static_cast<Uint32>(img.w);
    tex_info.height = static_cast<Uint32>(img.h);
    tex_info.layer_count_or_depth = 1;
    tex_info.num_levels = 1;
    tex_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
  }
  auto* tex = SDL_CreateGPUTexture(device, &tex_info);
  if (!tex) {
    LOG_ERROR("Couldn't create texture: {}", SDL_GetError());
    return tex;
  }

// TODO: raii containers for sdl resources please please please
#define ERR                                                                    \
  SDL_ReleaseGPUTexture(device, tex);                                          \
  return nullptr;
  SDL_GPUTransferBufferCreateInfo tr_info{};
  {
    tr_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tr_info.size = (Uint32)img.w * (Uint32)img.h * 4;
  };
  SDL_GPUTransferBuffer* trBuf = SDL_CreateGPUTransferBuffer(device, &tr_info);
  if (!trBuf) {
    LOG_ERROR("Couldn't create transfer buffer: {}", SDL_GetError);
    ERR
  }

  void* memory = SDL_MapGPUTransferBuffer(device, trBuf, false);
  if (!memory) {
    LOG_ERROR("Couldn't map texture memory: {}", SDL_GetError());
    SDL_ReleaseGPUTransferBuffer(device, trBuf);
    ERR
  }
  SDL_memcpy(memory, img.data, img.w * img.h * 4);
  SDL_UnmapGPUTransferBuffer(device, trBuf);

  SDL_GPUTextureTransferInfo tex_transfer_info{};
  tex_transfer_info.transfer_buffer = trBuf;
  tex_transfer_info.offset = 0;

  SDL_GPUTextureRegion tex_reg{};
  {
    tex_reg.texture = tex;
    tex_reg.w = (Uint32)img.w;
    tex_reg.h = (Uint32)img.h;
    tex_reg.d = 1;
  }

  {
    SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(device);
    if (!cmdBuf) {
      LOG_ERROR("Couldn't acquire command buffer: {}", SDL_GetError());
      SDL_ReleaseGPUTransferBuffer(device, trBuf);
      ERR
    }
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuf);
    SDL_UploadToGPUTexture(copyPass, &tex_transfer_info, &tex_reg, false);

    SDL_EndGPUCopyPass(copyPass);
    if (!SDL_SubmitGPUCommandBuffer(cmdBuf)) {
      LOG_ERROR("Couldn't sumbit command buffer: {}", SDL_GetError());
      SDL_ReleaseGPUTransferBuffer(device, trBuf);
      ERR
    }
    SDL_ReleaseGPUTransferBuffer(device, trBuf);
  }
#undef ERR
  LOG_DEBUG("Created texture and uploaded image data successfully");
  return tex;
}

bool
GLTFLoader::LoadSamplers()
{
  LOG_TRACE("GLTFLoader::LoadSamplers");

  samplers_ = std::vector<SDL_GPUSampler*>{};
  samplers_.reserve(asset_.samplers.size());

  if (asset_.samplers.empty()) {
    LOG_WARN("GLTF asset has no samplers. falling back to default");
    samplers_.push_back(default_sampler_);
    return true;
  }

  // auto near = fastgltf::Filter::Nearest;
  auto linear = fastgltf::Filter::Linear;
  for (auto& sampler : asset_.samplers) {
    SDL_GPUSamplerCreateInfo info{};
    {
      info.min_filter = extract_filter(sampler.minFilter.value_or(linear));
      info.mag_filter = extract_filter(sampler.magFilter.value_or(linear));
      info.mipmap_mode =
        extract_mipmap_mode(sampler.minFilter.value_or(linear));
      info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
      info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
      info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    }
    auto* s = SDL_CreateGPUSampler(program_->Device, &info);
    if (!s) {
      LOG_ERROR("Couldn't create gpu sampler: {}", SDL_GetError());
      s = default_sampler_;
    }
    samplers_.push_back(s);
  }
  assert(samplers_.size() == asset_.samplers.size());
  return true;
}

bool
GLTFLoader::CreateDefaultSampler()
{
  LOG_TRACE("GLTFLoader::CreateDefaultSampler");
  if (default_sampler_) {
    LOG_WARN("Re-creating default sampler");
  }
  static SDL_GPUSamplerCreateInfo sampler_info{};
  { // Matches GLTF default spec. Linear mipmap + linear filter = trilinear
    // filtering
    sampler_info.min_filter = SDL_GPU_FILTER_LINEAR;
    sampler_info.mag_filter = SDL_GPU_FILTER_LINEAR;
    sampler_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
    sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    sampler_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
  }
  SDL_GPUSampler* s = SDL_CreateGPUSampler(program_->Device, &sampler_info);
  if (!s) {
    LOG_ERROR("Default sampler creation failed: {}", SDL_GetError());
    return false;
  }
  default_sampler_ = s;
  return true;
}

bool
GLTFLoader::CreateDefaultTexture()
{
  LOG_TRACE("GLTFLoader::CreateDefaultTexture");
  if (default_texture_) {
    LOG_WARN("Re-creating default texture");
  }

  { // Create texture
    SDL_GPUTextureCreateInfo tex_info{};
    {
      tex_info.type = SDL_GPU_TEXTURETYPE_2D;
      tex_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
      tex_info.width = 32;
      tex_info.height = 32;
      tex_info.layer_count_or_depth = 1;
      tex_info.num_levels = 1;
      tex_info.usage =
        SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    }
    SDL_GPUTexture* t = SDL_CreateGPUTexture(program_->Device, &tex_info);

    if (!t) {
      LOG_ERROR("Default texture creation failed: {}", SDL_GetError());
      return false;
    }
    default_texture_ = t;
  }

  { // Clear texture
    SDL_GPUColorTargetInfo colorInfo{};
    {
      colorInfo.texture = default_texture_;
      colorInfo.clear_color = { 0.8f, 0.1f, 0.8f, 1.0f };
      colorInfo.load_op = SDL_GPU_LOADOP_CLEAR;
      colorInfo.store_op = SDL_GPU_STOREOP_STORE;
    };
    auto* cmdbuf = SDL_AcquireGPUCommandBuffer(program_->Device);
    if (!cmdbuf) {
      LOG_ERROR("Couldn't acquire command buffer: {}", SDL_GetError());
      return false;
    }
    SDL_GPURenderPass* renderPass =
      SDL_BeginGPURenderPass(cmdbuf, &colorInfo, 1, NULL);
    SDL_EndGPURenderPass(renderPass);

    if (!SDL_SubmitGPUCommandBuffer(cmdbuf)) {
      LOG_ERROR("Couldn't submit command buffer: {}", SDL_GetError());
      return false;
    }
  }

  return true;
}

bool
GLTFLoader::LoadMaterials()
{
  LOG_TRACE("GLTFLoader::LoadMaterials");
  if (asset_.materials.size() == 0) {
    LOG_WARN("No materials found in GLTF");
    materials_.push_back(default_material_);
    return true;
  }
  materials_ = std::vector<SharedPtr<GLTFPbrMaterial>>{};
  materials_.reserve(asset_.materials.size());

  for (auto& mat : asset_.materials) {
    auto newMat = std::make_shared<GLTFPbrMaterial>();

    { // factors
      newMat->BaseColorFactor.x = mat.pbrData.baseColorFactor.x();
      newMat->BaseColorFactor.y = mat.pbrData.baseColorFactor.y();
      newMat->BaseColorFactor.z = mat.pbrData.baseColorFactor.z();
      newMat->BaseColorFactor.w = mat.pbrData.baseColorFactor.w();
      newMat->EmissiveFactor.x = mat.emissiveFactor.x();
      newMat->EmissiveFactor.y = mat.emissiveFactor.y();
      newMat->EmissiveFactor.z = mat.emissiveFactor.z();
      newMat->MetallicFactor = mat.pbrData.metallicFactor;
      newMat->RoughnessFactor = mat.pbrData.roughnessFactor;
    }

    // template optional because fastgltf::NormalTextureInfo type is derived
    // from fastgltf::TextureInfo
    // auto bindTexture = [&](const auto& opt_tex_info, PbrTextureFlag tex_type)
    // {
    //   // using TexInfoType = std::decay_t<decltype(*opt_tex_info)>;
    //   if (opt_tex_info.has_value()) {
    //     auto tex_idx = opt_tex_info.value().textureIndex;
    //     assert(tex_idx < textures_.size());
    //
    //     auto sampler_idx = asset_.textures[tex_idx].samplerIndex.value_or(0);
    //     assert(sampler_idx < samplers_.size());
    //     newMat->SamplerBindings[CAST_FLAG(tex_type)] =
    //       SDL_GPUTextureSamplerBinding{ textures_[tex_idx],
    //                                     samplers_[sampler_idx] };
    //   } else {
    //     newMat->SamplerBindings[CAST_FLAG(tex_type)] =
    //       SDL_GPUTextureSamplerBinding{ default_texture_, default_sampler_ };
    //   }
    // };
    // bindTexture(mat.pbrData.baseColorTexture, PbrTextureFlag::BaseColor);

    if (mat.pbrData.baseColorTexture.has_value()) {
      auto tex_idx = mat.pbrData.baseColorTexture.value().textureIndex;
      assert(tex_idx < textures_.size());

      auto sampler_idx = asset_.textures[tex_idx].samplerIndex.value_or(0);
      assert(sampler_idx < samplers_.size());
      newMat->BaseColorTexture = textures_[tex_idx];
      newMat->BaseColorSampler = samplers_[sampler_idx];
    } else {
      newMat->BaseColorTexture = default_texture_;
      newMat->BaseColorSampler = default_sampler_;
    }
    if (mat.pbrData.metallicRoughnessTexture.has_value()) {
      auto tex_idx = mat.pbrData.metallicRoughnessTexture.value().textureIndex;
      assert(tex_idx < textures_.size());

      auto sampler_idx = asset_.textures[tex_idx].samplerIndex.value_or(0);
      assert(sampler_idx < samplers_.size());
      newMat->MetalRoughTexture = textures_[tex_idx];
      newMat->MetalRoughSampler = samplers_[sampler_idx];
    } else {
      newMat->MetalRoughTexture = default_texture_;
      newMat->MetalRoughSampler = default_sampler_;
    }
    if (mat.normalTexture.has_value()) {
      auto tex_idx = mat.normalTexture.value().textureIndex;
      assert(tex_idx < textures_.size());

      auto sampler_idx = asset_.textures[tex_idx].samplerIndex.value_or(0);
      assert(sampler_idx < samplers_.size());
      newMat->NormalTexture = textures_[tex_idx];
      newMat->NormalSampler = samplers_[sampler_idx];
    } else {
      newMat->NormalTexture = default_texture_;
      newMat->NormalSampler = default_sampler_;
    }

    materials_.push_back(newMat);
  }
  LOG_DEBUG("Loaded {} materials", materials_.size());
  return true;
}

bool
GLTFLoader::LoadNodes()
{
  LOG_TRACE("GLTFLoader::CreateDefaultMaterial");
  if (asset_.nodes.size() == 0) {
    LOG_ERROR("GLTF has no nodes (TODO: handle it as it's valid)")
    return false;
  }
  ParentNodes = std::vector<SharedPtr<SceneNode>>{};
  AllNodes = std::vector<SharedPtr<SceneNode>>{};
  AllNodes.reserve(asset_.nodes.size());

  // Create node for every node in scene
  for (const fastgltf::Node& node : asset_.nodes) {
    std::shared_ptr<SceneNode> NewNode;

    if (node.meshIndex.has_value()) {
      NewNode = std::make_shared<MeshNode>();
      auto idx = node.meshIndex.value();
      static_cast<MeshNode*>(NewNode.get())->Mesh = &meshes_[idx];
    } else {
      NewNode = std::make_shared<SceneNode>();
    }

    std::visit(fastgltf::visitor{
                 [&](fastgltf::math::fmat4x4 matrix) {
                   memcpy(&NewNode->LocalMatrix, matrix.data(), sizeof(matrix));
                 },
                 [&](fastgltf::TRS transform) {
                   glm::vec3 tl(transform.translation[0],
                                transform.translation[1],
                                transform.translation[2]);
                   glm::quat rot(transform.rotation[3],
                                 transform.rotation[0],
                                 transform.rotation[1],
                                 transform.rotation[2]);
                   glm::vec3 sc(transform.scale[0],
                                transform.scale[1],
                                transform.scale[2]);

                   glm::mat4 tm = glm::translate(glm::mat4(1.f), tl);
                   glm::mat4 rm = glm::toMat4(rot);
                   glm::mat4 sm = glm::scale(glm::mat4(1.f), sc);

                   NewNode->LocalMatrix = tm * rm * sm;
                 } },
               node.transform);
    AllNodes.push_back(NewNode);
  }

  // Iterate again to build hierarchy
  for (u64 i = 0; i < asset_.nodes.size(); ++i) {
    const fastgltf::Node& gltfnode = asset_.nodes[i];
    const SharedPtr<SceneNode>& node = AllNodes[i];

    for (const u64 childIdx : gltfnode.children) {
      node->Children.push_back(AllNodes[childIdx]);
      AllNodes[childIdx]->Parent = node;
    }
  }

  // Iterate again to identify Parent Nodes
  for (const auto& node : AllNodes) {
    if (node->Parent.lock() == nullptr) {
      ParentNodes.push_back(node);
      node->Update(glm::mat4{ 1.0f }); // Bubble down world matrix computation
    }
  }

  LOG_DEBUG("Loaded GLTF Nodes ({} parent, {} total)",
            ParentNodes.size(),
            AllNodes.size());

  return true;
}

void
GLTFLoader::Draw(glm::mat4 matrix, std::vector<RenderItem>& draws)
{
  for (const auto& parent : ParentNodes) {
    parent->Draw(matrix, draws);
  }
}

void
GLTFLoader::CreateDefaultMaterial()
{
  LOG_TRACE("GLTFLoader::CreateDefaultMaterial");
  if (default_material_.get() != nullptr) {
    LOG_WARN("Re-creating default material");
  }
  default_material_ = std::make_shared<GLTFPbrMaterial>();
}
