#pragma once

#include <filesystem>

class ScenePicker
{
public:
  explicit ScenePicker(std::filesystem::path base_path,
                       std::filesystem::path current_asset);
  void Render(bool loading);
  // void Render(std::function<void(std::filesystem::path)> callback);

public:
  static constexpr std::string GLB_EXT{ ".glb" };
  static constexpr std::string GLTF_EXT{ ".gltf" };
  std::filesystem::path CurrentAsset;

private:
  std::filesystem::path base_dir_;
  std::filesystem::path cwd_;
};
