#pragma once

#include "fastgltf/types.hpp"
#include "src/program.h"
#include "src/util.h"
#include "types.h"
#include <fastgltf/core.hpp>
#include <vector>

struct Geometry
{
  const std::size_t FirstIndex;
  const std::size_t VertexCount;
};
struct LoadedImage
{
  int w, h,
    nrChannels; // NOTE: don't use! Channel count is always forced to 4
  u8* data{ nullptr };
};

struct MeshAsset
{
  const char* Name;
  std::vector<Geometry> Submeshes;

  // TODO: don't duplicate these, send them to GPU directly when loading
  std::vector<PosUvVertex> vertices_{};
  std::vector<u32> indices_{};
};

class GLTFLoader
{
public:
  GLTFLoader(Program* program, std::filesystem::path path);
  ~GLTFLoader();

  bool Load();
  const std::vector<MeshAsset>& Meshes() const;
  const std::vector<SDL_GPUTexture*>& Textures() const;
  const std::vector<SDL_GPUSampler*>& Samplers() const;
  void Release();

private:
  bool LoadVertexData();

  bool LoadImageData();
  void LoadImageFromURI(LoadedImage& img, const fastgltf::sources::URI& URI);
  void LoadImageFromVector(LoadedImage& img,
                           const fastgltf::sources::Vector& vector);
  void LoadImageFromBufferView(LoadedImage& img,
                               const fastgltf::BufferView& view,
                               const fastgltf::Buffer& buffer);
  SDL_GPUTexture* CreateAndUploadTexture(LoadedImage& img);
  bool CreateDefaultTexture();

  bool LoadSamplers();
  bool CreateDefaultSampler();

private:
  Program* program_; // TODO: decouple program from renderer, and hold pointer
                     // to renderer instead
  fastgltf::Asset asset_;
  std::filesystem::path path_;
  bool loaded_{ false };

  SDL_GPUSampler* default_sampler_{ nullptr };
  SDL_GPUTexture* default_texture_{ nullptr };

  std::vector<MeshAsset> meshes_;
  std::vector<SDL_GPUTexture*> textures_;
  std::vector<SDL_GPUSampler*> samplers_;
};
