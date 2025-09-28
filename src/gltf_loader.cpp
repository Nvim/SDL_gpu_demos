#include "gltf_loader.h"
#include "logger.h"
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_gpu.h>
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
  for (auto tex : textures_) {
    RELEASE_IF(tex, SDL_ReleaseGPUTexture);
  }
  for (auto sampler : samplers_) {
    RELEASE_IF(sampler, SDL_ReleaseGPUSampler);
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

  loaded_ = LoadVertexData();
  if (!loaded_) {
    LOG_ERROR("Couldn't load vertex data from GLTF");
    return false;
  }
  LOG_DEBUG("Loaded GLTF meshes");

  loaded_ = LoadSamplers();
  if (!loaded_) {
    LOG_ERROR("Couldn't load samplers from GLTF");
    return false;
  }
  LOG_DEBUG("Loaded GLTF samplers");

  loaded_ = LoadImageData();
  if (!loaded_) {
    LOG_ERROR("Couldn't load images from GLTF");
    return false;
  }
  LOG_DEBUG("Loaded GLTF images");

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

  for (auto& mesh : asset_.meshes) {
    MeshAsset newMesh;
    newMesh.Name = mesh.name.c_str();
    auto& indices = newMesh.indices_;
    auto& vertices = newMesh.vertices_;

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
            PosUvVertex newvtx;
            newvtx.pos[0] = v.x;
            newvtx.pos[1] = v.y;
            newvtx.pos[2] = v.z;
            newvtx.uv[0] = 0;
            newvtx.uv[1] = 0;
            vertices[initial_vtx + index] = newvtx;
          });
      }

      { // load vertex UVs
        auto attr = p.findAttribute("TEXCOORD_0");
        if (attr != p.attributes.end()) {
          fastgltf::iterateAccessorWithIndex<glm::vec2>(
            asset_,
            asset_.accessors[(*attr).accessorIndex],
            [&](glm::vec2 v, size_t index) {
              vertices[initial_vtx + index].uv[0] = v.x;
              vertices[initial_vtx + index].uv[1] = v.y;
            });
        }
      }

      newMesh.Submeshes.push_back(newGeometry);
      LOG_DEBUG("New geometry. Total Verts: {}, Total Indices: {} || {}, {}",
                vertices.size(),
                indices.size(),
                newMesh.Submeshes[0].FirstIndex,
                newMesh.Submeshes[0].VertexCount);
    }
    meshes_.emplace_back(newMesh);
    assert(newMesh.indices_.size() != 0);
  }
  assert(meshes_[0].indices_.size() != 0);
  return true;
}

bool
GLTFLoader::LoadImageData()
{
  LOG_TRACE("GLTFLoader::LoadImageData");
  if (asset_.images.empty()) {
    LOG_WARN("LoadImageData: GLTF has no images");
    return false;
  }

  textures_ = std::vector<SDL_GPUTexture*>{};
  textures_.reserve(asset_.textures.size());

  u32 tex_idx =
    asset_.materials[0].pbrData.baseColorTexture.value().textureIndex;
  assert(tex_idx == 0);
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

  LOG_DEBUG("{} textures were created", textures_.size());
  assert(textures_.size() == asset_.textures.size());
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
    LOG_WARN("asset has no samplers. falling back to default");
    samplers_.push_back(default_sampler_);
    return true;
  }

  auto near = fastgltf::Filter::Nearest;
  for (auto& sampler : asset_.samplers) {
    SDL_GPUSamplerCreateInfo info{};
    {
      info.min_filter = extract_filter(sampler.minFilter.value_or(near));
      info.mag_filter = extract_filter(sampler.magFilter.value_or(near));
      info.mipmap_mode = extract_mipmap_mode(sampler.minFilter.value_or(near));
      info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
      info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
      info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
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
  {
    sampler_info.min_filter = SDL_GPU_FILTER_NEAREST;
    sampler_info.mag_filter = SDL_GPU_FILTER_NEAREST;
    sampler_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
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
