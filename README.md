# SDL3 GPU is pretty good

## Apps

The repository contains two (WIP) apps for now:
- `pbr`: GLTF model viewer, with supports for _most_ features from the [GLTF PBR spec](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)
- `grass`: A dense, real-time grassblade field simulation aiming to offload as much as possible to the GPU (terrain and grassblade generation, frustum culling and draw-call generation are all GPU-driven thanks to compute shaders)

### Common features

- Minimal framework and collection of utilities to reduce boilerplate
- Support for HDR textures and cubemaps
- Support for KTX file format
- Image based lighting
- Multiple HDR post-processing shaders
- Generation of missing tangents and bitangents at runtime

## build

### Program

The project uses CMake Presets for easy configuration:

```bash
mkdir build
cmake --preset rel-ninja # or `debug-ninja`/`debug-asan`
cmake --build --preset release # or `debug`/`asan`
```

You can then run it:
```
build/rel-ninja/sdl_demos [APP_NAME]
```

Valid app names are `pbr` and `grass`.

The following libraries are expected to be installed system-wide:
- [SDL3](https://github.com/libsdl-org/SDL) (you can choose to build by defining the `BUILD_SDL` CMake option)
- [glm](https://github.com/g-truc/glm)
- [simdjson](https://github.com/simdjson/simdjson) (it's supposed to be fetched & built as a dependency of fastgltf, but it doesn't work for me)

These are built along with the project (make sure to clone submodules):
- [spdlog](https://github.com/gabime/spdlog.git)
- [stb](https://github.com/nothings/stb.git)
- [imgui](https://github.com/ocornut/imgui/)
- [fastgltf](https://github.com/spnda/fastgltf)
- [Catch2](https://github.com/catchorg/Catch2/)
- [ktx](https://github.com/KhronosGroup/KTX-Software)
- [mikktspace](https://github.com/mmikk/MikkTSpace)


### Shaders

Use the `shaders.sh` script to build the shaders related to the app you want to run:

```bash
# TODO: add this as a CMake target someday
./shaders.sh grass
./shaders.sh pbr
```
