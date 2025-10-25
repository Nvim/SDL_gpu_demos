#include <pch.h>

#include "common/gltf_loader.h"

#include "common/material.h"
#include "common/rendersystem.h"
#include "common/tangent_loader.h"
#include "common/types.h"
#include "common/util.h"
#include "shaders/material_features.h"

#include <limits>
#include <variant>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/math.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#define GLM_ENABLE_EXPERIMENTAL 1
#include <glm/ext/quaternion_common.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/gtx/quaternion.hpp>
#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#endif
#include <stb/stb_image.h>

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

GLTFLoader::GLTFLoader(Program* program)
  : program_{ program }
{

  tangent_loader_ = std::make_unique<OGLDevTangentLoader>();

  if (!CreatePipelines()) {
    LOG_ERROR("Couldn't create default sampler");
    return;
  }

  if (!CreateDefaultSampler()) {
    LOG_ERROR("Couldn't create default sampler");
    return;
  }
  if (!CreateDefaultTexture()) {
    LOG_ERROR("Couldn't create default texture");
    return;
  }
  CreateDefaultMaterial();
}

bool
GLTFLoader::IsInitialized()
{
  // clang-format off
  return (
    program_ != nullptr &&
    tangent_loader_ != nullptr &&
    opaque_pipeline_ != nullptr &&
    transparent_pipeline_ != nullptr &&
    default_sampler_ != nullptr &&
    default_texture_ != nullptr &&
    default_material_ != nullptr
  );
  // clang-format on
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
  RELEASE_IF(default_texture_, SDL_ReleaseGPUTexture);
  RELEASE_IF(default_sampler_, SDL_ReleaseGPUSampler);

  LOG_DEBUG("Released GLTFLoader resources");
}

UniquePtr<GLTFScene>
GLTFLoader::Load(std::filesystem::path& path)
{
  LOG_TRACE("GLTFLoader::Load");
  if (!std::filesystem::exists(path)) {
    LOG_ERROR("path {} is invalid", path.c_str());
    return nullptr;
  }

  if (!IsInitialized()) {
    LOG_ERROR("Loader isn't initalized");
    return nullptr;
  }

  if (!Parse(path)) {
    LOG_ERROR("Coudln't parse asset `{}`", path.c_str())
    return nullptr;
  }

  UniquePtr<GLTFScene> ret = MakeUnique<GLTFScene>(path, this);

  if (!LoadResources(ret.get())) {
    return nullptr;
  }
  return ret;
}

bool
GLTFLoader::Load(GLTFScene* scene, std::filesystem::path& path)
{
  LOG_TRACE("GLTFLoader::Load");
  if (!std::filesystem::exists(path)) {
    LOG_ERROR("path {} is invalid", path.c_str());
    return false;
  }

  if (!transparent_pipeline_ || !opaque_pipeline_) {
    LOG_ERROR("Default pipelines aren't initalized");
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

  if (!Parse(path)) {
    LOG_ERROR("Coudln't parse asset `{}`", path.c_str())
    return false;
  }

  scene->Path = path;
  scene->loader_ = this;
  return LoadResources(scene);
}

bool
GLTFLoader::Parse(std::filesystem::path& path)
{
  auto data = fastgltf::GltfDataBuffer::FromPath(path);
  if (data.error() != fastgltf::Error::None) {
    LOG_ERROR("couldn't load gltf from path");
    return false;
  }

  constexpr auto gltfOptions = fastgltf::Options::LoadExternalBuffers;

  fastgltf::Asset gltf;
  fastgltf::Parser parser{};

  auto asset = parser.loadGltf(data.get(), path.parent_path(), gltfOptions);
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
  return true;
}

bool
GLTFLoader::LoadResources(GLTFScene* ret)
{
  if (!LoadSamplers(ret)) {
    LOG_ERROR("Couldn't load samplers from GLTF");
    return false;
  }
  LOG_DEBUG("GLTFLoader: Loaded {} Samplers", ret->samplers_.size());

  // if (!LoadImageData(ret)) {
  //   LOG_ERROR("Couldn't load images from GLTF");
  //   return false;
  // }
  ret->textures_ = std::vector<SDL_GPUTexture*>{ asset_.textures.size() };

  if (!LoadMaterials(ret)) {
    LOG_ERROR("Couldn't load materials from GLTF");
    return false;
  }
  LOG_DEBUG("GLTFLoader: Loaded {} Materials", ret->materials_.size());

  if (!LoadVertexData(ret)) {
    LOG_ERROR("Couldn't load vertex data from GLTF");
    return false;
  }
  LOG_DEBUG("GLTFLoader: Loaded {} Meshes", ret->meshes_.size());

  if (!LoadNodes(ret)) {
    LOG_ERROR("Couldn't load nodes from GLTF");
    return false;
  }
  LOG_DEBUG("GLTFLoader: Loaded {} parent nodes ({} total)",
            ret->parent_nodes_.size(),
            ret->all_nodes_.size());

  ret->loaded_ = true;
  return true;
}

bool
GLTFLoader::LoadVertexData(GLTFScene* ret)
{
  LOG_TRACE("GLTFLoader::LoadVertexData");
  if (asset_.meshes.empty()) {
    LOG_WARN("LoadVertexData: GLTF has no meshes");
    return false;
  }
  // construct our Mesh vector at the same time we fill vertices and indices
  // buffers
  ret->meshes_ = std::vector<MeshAsset>{};
  ret->meshes_.reserve(asset_.meshes.size());
  std::vector<PosNormalTangentColorUvVertex> vertices;
  std::vector<u32> indices;

  for (auto& mesh : asset_.meshes) {
    MeshAsset newMesh;
    newMesh.Name = mesh.name.c_str();
    vertices.clear();
    indices.clear();
    bool need_tangents{ false };

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

      { // load positions
        fastgltf::Accessor& posAccessor =
          asset_.accessors[p.findAttribute("POSITION")->accessorIndex];
        vertices.resize(vertices.size() + posAccessor.count);

        fastgltf::iterateAccessorWithIndex<glm::vec3>(
          asset_, posAccessor, [&](glm::vec3 v, size_t index) {
            vertices[initial_vtx + index].pos = v;
          });
      }

      { // load normals:
        auto attr = p.findAttribute("NORMAL");
        if (attr != p.attributes.end()) {
          fastgltf::iterateAccessorWithIndex<glm::vec3>(
            asset_,
            asset_.accessors[attr->accessorIndex],
            [&](glm::vec3 v, size_t index) {
              vertices[initial_vtx + index].normal = v;
            });
        }
      }

      { // load color:
        auto attr = p.findAttribute("COLOR_0");
        if (attr != p.attributes.end()) {
          fastgltf::iterateAccessorWithIndex<glm::vec4>(
            asset_,
            asset_.accessors[attr->accessorIndex],
            [&](glm::vec4 v, size_t index) {
              vertices[initial_vtx + index].color = v;
            });
        }
      }

      { // load UVs
        auto attr = p.findAttribute("TEXCOORD_0");
        if (attr != p.attributes.end()) {
          fastgltf::iterateAccessorWithIndex<glm::vec2>(
            asset_,
            asset_.accessors[attr->accessorIndex],
            [&](glm::vec2 v, size_t index) {
              vertices[initial_vtx + index].uv = v;
            });
        }
      }

      { // material:
        if (p.materialIndex.has_value()) {
          auto& mat = ret->materials_[p.materialIndex.value()];
          newGeometry.material = mat->Build();
          if (!need_tangents) {
            need_tangents = (mat->FeatureFlags & HAS_NORMAL_TEX);
          }
        } else {
          // TODO: pre-build default material once
          newGeometry.material = ret->materials_[0]->Build();
        }
        if (newGeometry.material->Opacity == MaterialOpacity::Opaque) {
          newGeometry.material->Pipeline = opaque_pipeline_;
        } else {
          newGeometry.material->Pipeline = transparent_pipeline_;
        }
      }

      { // tangents:
        auto attr = p.findAttribute("TANGENT");
        if (attr != p.attributes.end()) {
          fastgltf::iterateAccessorWithIndex<glm::vec4>(
            asset_,
            asset_.accessors[attr->accessorIndex],
            [&](glm::vec4 v, size_t index) {
              vertices[initial_vtx + index].tangent = v;
            });
          LOG_INFO("Found tangents attribute for mesh {}", mesh.name.c_str());
          need_tangents = false;
        }
      }

      newMesh.Submeshes.push_back(newGeometry);
      LOG_DEBUG("New geometry. Total Verts: {}, Total Indices: {}",
                vertices.size(),
                indices.size());
    }

    // Lazy pre-compute tangents only if necessary:
    // Iterate all vertices after they're all loaded
    if (need_tangents) {
      LOG_INFO("Computing tangents for mesh {}", mesh.name.c_str());
      CPUMeshBuffers buffers{};
      {
        buffers.IndexBuffer = &indices;
        buffers.VertexBuffer = &vertices;
      }
      tangent_loader_->Load(&buffers);
    }
    if (!UploadBuffers(&newMesh.Buffers, vertices, indices)) {
      LOG_ERROR("Couldn't upload mesh data");
      return false;
    }
    ret->meshes_.emplace_back(newMesh);
  }
  return true;
}

bool
GLTFLoader::LoadTexture(GLTFScene* ret, u64 texture_index, bool srgb)
{
  LOG_TRACE("GLTFLoader::LoadTexture");

  if (texture_index >= asset_.textures.size()) {
    return false;
  }
  auto& tex = asset_.textures[texture_index];
  u64 img_idx = tex.imageIndex.value_or(std::numeric_limits<u64>::max());
  if (img_idx >= asset_.images.size()) {
    return false;
  }
  auto& img = asset_.images[img_idx];
  LoadedImage imgData{};
  { // load image data to CPU
    std::visit(fastgltf::visitor{
                 // clang-format off
        []([[maybe_unused]] auto& arg) {LOG_WARN("No URI source");},
        [&](fastgltf::sources::URI& filePath) {
          LOG_DEBUG("Loading image from URI");
          LoadImageFromURI(imgData, ret->Path.parent_path(), filePath);
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
      }, img.data);
    // clang-format on
  }

  { // create GPU texture
    SDL_GPUTexture* tex{ nullptr };
    if (imgData.data) {
      LOG_DEBUG("Creating texture");
      tex = CreateAndUploadTexture(imgData, srgb);
      stbi_image_free(imgData.data);
    }
    if (!tex) {
      LOG_WARN("Falling back to default textue");
      tex = default_texture_;
    }
    // ret->textures_.push_back(tex);
    ret->textures_[texture_index] = tex;
  }
  return true;
}

// bool
// GLTFLoader::LoadImageData(GLTFScene* ret)
// {
//   LOG_TRACE("GLTFLoader::LoadImageData");
//
//   ret->textures_ = std::vector<SDL_GPUTexture*>{};
//   ret->textures_.reserve(asset_.textures.size());
//
//   if (asset_.images.empty()) {
//     LOG_WARN("LoadImageData: GLTF has no images");
//     ret->textures_.push_back(default_texture_);
//     return true;
//   }
//
//   for (auto& image : asset_.images) {
//     LoadedImage imgData{};
//     { // load image data to CPU
//       std::visit(fastgltf::visitor{
//                    // clang-format off
//         []([[maybe_unused]] auto& arg) {LOG_WARN("No URI source");},
//         [&](fastgltf::sources::URI& filePath) {
//           LOG_DEBUG("Loading image from URI");
//           LoadImageFromURI(imgData, ret->Path.parent_path(), filePath);
//         },
//         [&](fastgltf::sources::Vector& vec) {
//           LOG_DEBUG("Loading image from Vector");
//           LoadImageFromVector(imgData,vec);
//         },
//         [&](fastgltf::sources::BufferView& view) {
//           const auto& bufferView = asset_.bufferViews[view.bufferViewIndex];
//           const auto& buffer = asset_.buffers[bufferView.bufferIndex];
//
//           LoadImageFromBufferView(imgData, bufferView, buffer);
//         }
//       }, image.data);
//       // clang-format on
//     }
//
//     { // create GPU texture TODO: need to be done in material loading for
//       // rgb/srgb
//       SDL_GPUTexture* tex{ nullptr };
//       if (imgData.data) {
//         LOG_DEBUG("Creating texture");
//         tex = CreateAndUploadTexture(imgData);
//         stbi_image_free(imgData.data);
//       }
//       if (!tex) {
//         LOG_WARN("Falling back to default textue");
//         tex = default_texture_;
//       }
//       ret->textures_.push_back(tex);
//     }
//   }
//
//   // random asserts to delete
//   for (const auto& tex : asset_.textures) {
//     if (!tex.imageIndex.has_value()) {
//       LOG_WARN("Texture doesn't have image.");
//       if (tex.basisuImageIndex.has_value()) {
//         LOG_WARN("It has a baseiu");
//       }
//       if (tex.ddsImageIndex.has_value()) {
//         LOG_WARN("It has a dds");
//       }
//       if (tex.webpImageIndex.has_value()) {
//         LOG_WARN("It has a webp");
//       }
//     }
//   }

// LOG_DEBUG("{} textures were created", ret->textures_.size());
// assert(ret->textures_.size() == asset_.images.size());
// return !ret->textures_.empty();
// }

void
GLTFLoader::LoadImageFromURI(LoadedImage& img,
                             std::filesystem::path parent_path,
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
  img.data =
    stbi_load(std::format("{}/{}", parent_path.c_str(), localPath).c_str(),
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
GLTFLoader::CreateAndUploadTexture(LoadedImage& img, bool srgb)
{
  LOG_TRACE("GLTFLoader::CreateAndUploadTexture");
  auto device = program_->Device;
  auto format = srgb ? SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB
                     : SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

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
GLTFLoader::LoadSamplers(GLTFScene* ret)
{
  LOG_TRACE("GLTFLoader::LoadSamplers");

  ret->samplers_ = std::vector<SDL_GPUSampler*>{};
  ret->samplers_.reserve(asset_.samplers.size());

  if (asset_.samplers.empty()) {
    LOG_WARN("GLTF asset has no samplers. falling back to default");
    ret->samplers_.push_back(default_sampler_);
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
    ret->samplers_.push_back(s);
  }
  assert(ret->samplers_.size() == asset_.samplers.size());
  return true;
}

bool
GLTFLoader::CreateDefaultSampler()
{
  LOG_TRACE("GLTFLoader::CreateDefaultSampler");
  if (default_sampler_) {
    LOG_DEBUG("Skipping default sampler recreation");
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
    LOG_DEBUG("Skipping default texture recreation");
    return true;
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
GLTFLoader::LoadMaterials(GLTFScene* ret)
{
  LOG_TRACE("GLTFLoader::LoadMaterials");
  if (asset_.materials.size() == 0) {
    LOG_WARN("No materials found in GLTF");
    ret->materials_.push_back(default_material_);
    return true;
  }
  ret->materials_ = std::vector<SharedPtr<GLTFPbrMaterial>>{};
  ret->materials_.reserve(asset_.materials.size());

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

      // optional ones:
      if (mat.normalTexture.has_value()) {
        newMat->NormalFactor = mat.normalTexture.value().scale;
        newMat->FeatureFlags |= HAS_NORMAL_FACT;
      }
      if (mat.occlusionTexture.has_value()) {
        newMat->OcclusionFactor = mat.occlusionTexture.value().strength;
        newMat->FeatureFlags |= HAS_OCCLUSION_FACT;
      }

      // TODO: support AlphaMode::Mask too
      if (mat.alphaMode == fastgltf::AlphaMode::Blend) {
        newMat->opacity = MaterialOpacity::Transparent;
      }
    }

    // template optional because fastgltf::NormalTextureInfo type is derived
    // from fastgltf::TextureInfo
    auto bindTexture = [&](const auto& opt_tex_info,
                           SDL_GPUTexture** texture,
                           SDL_GPUSampler** sampler,
                           int flag,
                           [[maybe_unused]] bool srgb = false) {
      // using TexInfoType = std::decay_t<decltype(*opt_tex_info)>;
      if (opt_tex_info.has_value()) {
        auto tex_idx = opt_tex_info.value().textureIndex;
        // assert(tex_idx < ret->textures_.size());
        assert(LoadTexture(ret, tex_idx, srgb) == true);

        auto sampler_idx = asset_.textures[tex_idx].samplerIndex.value_or(0);
        assert(sampler_idx < ret->samplers_.size());
        *texture = ret->textures_[tex_idx];
        *sampler = ret->samplers_[sampler_idx];
        newMat->FeatureFlags |= flag;
      } else {
        *texture = default_texture_;
        *sampler = default_sampler_;
      }
    };

    bindTexture(mat.pbrData.baseColorTexture,
                &(newMat->BaseColorTexture),
                &(newMat->BaseColorSampler),
                HAS_DIFFUSE_TEX,
                true);
    bindTexture(mat.pbrData.metallicRoughnessTexture,
                &(newMat->MetalRoughTexture),
                &(newMat->MetalRoughSampler),
                HAS_METALROUGH_TEX);
    bindTexture(mat.normalTexture,
                &(newMat->NormalTexture),
                &(newMat->NormalSampler),
                HAS_NORMAL_TEX);

    bindTexture(mat.occlusionTexture,
                &(newMat->OcclusionTexture),
                &(newMat->OcclusionSampler),
                HAS_OCCLUSION_TEX);
    bindTexture(mat.emissiveTexture,
                &(newMat->EmissiveTexture),
                &(newMat->EmissiveSampler),
                HAS_EMISSIVE_TEX);

    // assert(newMat->FeatureFlags & HAS_DIFFUSE_TEX);
    ret->materials_.push_back(newMat);
  }
  LOG_DEBUG("Loaded {} materials", ret->materials_.size());
  return true;
}

bool
GLTFLoader::LoadNodes(GLTFScene* ret)
{
  LOG_TRACE("GLTFLoader::LoadNodes");
  if (asset_.nodes.size() == 0) {
    LOG_ERROR("GLTF has no nodes (TODO: handle it as it's valid)")
    return false;
  }
  ret->parent_nodes_ = std::vector<SharedPtr<SceneNode>>{};
  ret->all_nodes_ = std::vector<SharedPtr<SceneNode>>{};
  ret->all_nodes_.reserve(asset_.nodes.size());

  // Create node for every node in scene
  for (const fastgltf::Node& node : asset_.nodes) {
    std::shared_ptr<SceneNode> NewNode;

    if (node.meshIndex.has_value()) {
      NewNode = std::make_shared<MeshNode>();
      auto idx = node.meshIndex.value();
      static_cast<MeshNode*>(NewNode.get())->Mesh = &ret->meshes_[idx];
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
    ret->all_nodes_.push_back(NewNode);
  }

  // Iterate again to build hierarchy
  for (u64 i = 0; i < asset_.nodes.size(); ++i) {
    const fastgltf::Node& gltfnode = asset_.nodes[i];
    const SharedPtr<SceneNode>& node = ret->all_nodes_[i];

    for (const u64 childIdx : gltfnode.children) {
      node->Children.push_back(ret->all_nodes_[childIdx]);
      ret->all_nodes_[childIdx]->Parent = node;
    }
  }

  // Iterate again to identify Parent Nodes
  for (const auto& node : ret->all_nodes_) {
    if (node->Parent.lock() == nullptr) {
      ret->parent_nodes_.push_back(node);
      node->Update(glm::mat4{ 1.0f }); // Bubble down world matrix computation
    }
  }
  return true;
}

void
GLTFLoader::CreateDefaultMaterial()
{
  LOG_TRACE("GLTFLoader::CreateDefaultMaterial");
  if (default_material_.get() != nullptr) {
    LOG_DEBUG("Skipping default material recreation");
    return;
  }
  default_material_ = std::make_shared<GLTFPbrMaterial>();
}

bool
GLTFLoader::CreatePipelines()
{
  auto Device = program_->Device;
  auto Window = program_->Window;

  SDL_GPUShader* vs{ nullptr };
  SDL_GPUShader* fs{ nullptr };

  { // Shader loading
    SDL_GPUShaderFormat backendFormats = SDL_GetGPUShaderFormats(Device);
    if (!(backendFormats & SDL_GPU_SHADERFORMAT_SPIRV)) {
      LOG_ERROR("Backend doesn't support SPRIR-V");
      return false;
    }
    auto samplers = GLTFPbrMaterial::TextureCount;
    auto vertUbos = GLTFPbrMaterial::VertexUBOCount;
    auto fragUbos = GLTFPbrMaterial::FragmentUBOCount;
    vs = LoadShader(VertexShaderPath, Device, 0, vertUbos, 0, 0);
    if (vs == nullptr) {
      LOG_ERROR("Couldn't load vertex shader at path {}", VertexShaderPath);
      return false;
    }
    fs = LoadShader(FragmentShaderPath, Device, samplers, fragUbos, 0, 0);
    if (fs == nullptr) {
      LOG_ERROR("Couldn't load fragment shader at path {}", FragmentShaderPath);
      return false;
    }
  }

  // TODO: query client's available modes and select an HDR mode:
  SDL_GPUColorTargetDescription color_descs[1]{};
  {
    auto& d = color_descs[0];
    d.format = SDL_GetGPUSwapchainTextureFormat(Device, Window);
    d.blend_state.enable_blend = false;
  }

  SDL_GPUVertexBufferDescription vertex_desc[] = { {
    .slot = 0,
    .pitch = sizeof(PosNormalTangentColorUvVertex),
    .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
    .instance_step_rate = 0,
  } };

  SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo{};
  {
    pipelineCreateInfo.vertex_shader = vs;
    pipelineCreateInfo.fragment_shader = fs;
    pipelineCreateInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    {
      auto& state = pipelineCreateInfo.vertex_input_state;
      state.vertex_buffer_descriptions = vertex_desc;
      state.num_vertex_buffers = 1;
      state.vertex_attributes = PosNormalTangentColorUvAttributes;
      state.num_vertex_attributes = PosNormalTangentColorUvAttributeCount;
    }
    {
      auto& state = pipelineCreateInfo.rasterizer_state;
      state.fill_mode = SDL_GPU_FILLMODE_FILL,
      state.cull_mode = SDL_GPU_CULLMODE_NONE;
      state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    }
    {
      auto& state = pipelineCreateInfo.depth_stencil_state;
      state.compare_op = SDL_GPU_COMPAREOP_LESS;
      state.enable_depth_test = true;
      state.enable_depth_write = true;
      state.enable_stencil_test = false;
    }
    {
      auto& info = pipelineCreateInfo.target_info;
      info.color_target_descriptions = color_descs;
      info.num_color_targets = 1;
      info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
      info.has_depth_stencil_target = true;
    }
  }

  opaque_pipeline_ = SDL_CreateGPUGraphicsPipeline(Device, &pipelineCreateInfo);
  if (opaque_pipeline_ == nullptr) {
    LOG_ERROR("Couldn't create pipeline!");
    return false;
  }

  { // Enable blending
    auto& d = color_descs[0];
    d.format = SDL_GetGPUSwapchainTextureFormat(Device, Window);
    d.blend_state.enable_blend = true;
    d.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    d.blend_state.dst_color_blendfactor =
      SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    d.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    d.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    d.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    d.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
    // Transparent objects don't write to depth buffer
    pipelineCreateInfo.depth_stencil_state.enable_depth_write = false;
  }
  transparent_pipeline_ =
    SDL_CreateGPUGraphicsPipeline(Device, &pipelineCreateInfo);
  if (opaque_pipeline_ == nullptr) {
    LOG_ERROR("Couldn't create pipeline!");
    return false;
  }
  return true;
}
