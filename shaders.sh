#!/usr/bin/env bash
set -euo pipefail

SRC=src/shaders
OUT=resources/shaders/compiled

mkdir -p "$OUT"

glslang $SRC/frag.frag -V -o $OUT/frag.spv -I$SRC;
glslang $SRC/vert.vert -V -o $OUT/vert.spv -I$SRC;
glslang $SRC/skybox.frag -V -o $OUT/skybox.frag.spv;
glslang $SRC/skybox.vert -V -o $OUT/skybox.vert.spv -I$SRC;

## 
glslang $SRC/color.frag -V -o $OUT/color.frag.spv;
glslang $SRC/ssbo.vert -V -o $OUT/ssbo.vert.spv;
