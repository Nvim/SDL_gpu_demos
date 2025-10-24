#include <pch.h>

#include "scene_picker.h"

#include <imgui.h>

ScenePicker::ScenePicker(std::filesystem::path base_path,
                         std::filesystem::path current_asset)
  : CurrentAsset{ current_asset }
  , base_dir_{ base_path }
  , cwd_{ base_path }
{
  assert(cwd_.empty() == false);
}

void
ScenePicker::Render(bool loading)
// ScenePicker::Render(std::function<void(std::filesystem::path)> callback)
{
  if (cwd_.empty()) {
    return;
  }

  if (ImGui::Begin("Scene picker")) {

    ImGui::Text("current dir: %s", cwd_.c_str());

    if (loading == true) {
      ImGui::Text("Loading...");
      ImGui::End();
      return;
    }

    bool disable_back = cwd_ == base_dir_;
    if (disable_back) {
      ImGui::BeginDisabled();
    }
    if (ImGui::Button("< Back")) {
      cwd_ = cwd_.parent_path();
    }
    if (disable_back) {
      ImGui::EndDisabled();
    }

    for (const auto& it : std::filesystem::directory_iterator{ cwd_ }) {
      auto path = it.path();
      bool disable = (path == CurrentAsset);

      if (disable) {
        ImGui::BeginDisabled(disable);
      }
      if (it.is_directory()) {
        if (ImGui::Button(path.stem().c_str())) {
          cwd_ = cwd_ / path.stem();
        }
      } else if (path.has_extension()) {
        auto ext = path.extension();
        if (ext == GLTF_EXT || ext == GLB_EXT) {
          if (ImGui::Button(path.filename().c_str())) {
            CurrentAsset = path;
          }
        }
      }
      if (disable) {
        ImGui::EndDisabled();
      }
    }
    ImGui::End();
  }
}
