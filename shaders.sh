#!/usr/bin/env bash
set -euo pipefail

SRC=src/shaders
OUT=resources/shaders/compiled

mkdir -p "$OUT"

SHADER_LIST=( $(find $SRC -type f \( -name "*.comp" -o -name "*.frag" -o -name "*.vert" \) -exec basename {} \;) )

if [[ $# -eq 1 && -n $1 ]]; then 
  if [[ $1 = "pbr" ]]; then 
    SHADER_LIST=(
      "pbr.vert"
      "pbr.frag"
      "post_process.comp"
      ) 
  fi

  if [[  $1 = "grass" ]]; then 
    SHADER_LIST=(
      "cull_chunks.comp"
      "generate_grass.comp"
      "grass.vert"
      "terrain.vert"
      "terrain.frag"
      "grass.frag"
      "grid.vert"
      "grid.frag"
      ) 
  fi

  if [[ $1 = "base" ]]; then 
    SHADER_LIST=(
      "skybox.vert"
      "cubemap_projection.vert"
      "skybox.frag"
      "cubemap_projection.frag"
      "color.frag"
      ) 
  fi
fi


for shader in "${SHADER_LIST[@]}"; do
  glslang "$SRC/$shader" -V -o "$OUT/$shader.spv" "-I$SRC";
done
