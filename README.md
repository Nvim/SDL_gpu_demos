# SDL3 GPU is fun

Not much to see here

## build

### Program

```bash
mkdir build
cmake -B build -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=1
cmake --build build -j$(nproc)
```

The following libraries are expected to be installed system-wide:
- [SDL3](https://github.com/libsdl-org/SDL)
- [glm](https://github.com/g-truc/glm)
- [simdjson](https://github.com/simdjson/simdjson) (it's supposed to be fetched & built as a dependency of fastgltf, but it doesn't work for me)

These are built along with the project:
- [spdlog](https://github.com/gabime/spdlog.git)
- [stb](https://github.com/nothings/stb.git)
- [imgui](https://github.com/ocornut/imgui/)
- [fastgltf](https://github.com/spnda/fastgltf)

### Shaders

Use `glslang` or `glslc` to compile GLSL shaders to SPIR-V, SDL takes care of
the rest

```bash
# TODO: add this as a CMake target someday
./shaders.sh
```
