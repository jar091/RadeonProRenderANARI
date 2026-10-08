# RadeonProRender ANARI device
## Description
This is an implementation of an [ANARI](https://www.khronos.org/anari/) device based on the
[RadeonProRender](https://github.com/GPUOpen-LibrariesAndSDKs/RadeonProRenderSDK) engine
(Northstar plugin). The ANARI library is called `rpr` (`anari_library_rpr`).

The device is built on the helium layer of the ANARI SDK and follows ANARI 1.0. It was
tested with ANARI SDK 0.17 and RadeonProRender SDK 3.1.7.1 on Windows with an NVIDIA GPU.
The Linux and macOS builds and AMD GPUs (HIP) are not tested.
## Implementation details
### Device
* device parameters (set before the first object is created):
  * `computeDevice`: `gpu` (default), `cpu`, `gpu+cpu` or `opencl`
  * `gpuIndex`: index of the GPU used for rendering
  * `kernelsPath`: directory with the precompiled Northstar kernels (default: `hipbin` next to the device library)
  * `cachePath`: directory for the RadeonProRender kernel cache (default: `cache` next to the device library)
* the environment variables `ANARI_RPR_COMPUTE_DEVICE`, `ANARI_RPR_KERNELS_PATH` and `ANARI_RPR_CACHE_PATH` override these parameters
* if the GPU context cannot be created, the device warns and renders on the CPU
* the OpenCL back-end is only used when asked for; it is never a fallback
### Frame
* `channel.color` as `UFIXED8_RGBA_SRGB`, `UFIXED8_VEC4` or `FLOAT32_VEC4`, `channel.depth` as `FLOAT32`
* `ANARI_KHR_FRAME_ACCUMULATION`: with `accumulation` enabled the samples accumulate until something is committed
* other channels (normal, albedo, ids) are not supported
### Renderer
* subtype `default`: `background` (color or image), `ambientRadiance`, `ambientColor`, `pixelSamples`, `maxRayDepth`, `radianceClamp`
### Camera
* supported types: `perspective` (with depth of field) and `orthographic`
* `imageRegion` is not supported
### Geometry
* `triangle` and `quad` are RadeonProRender meshes
* `sphere` is rendered with instances of one sphere mesh
* `cylinder`, `cone` and `curve` are tessellated
* `isosurface` is extracted from `structuredRegular` fields
### Materials and samplers
* `matte` and `physicallyBased` use the RadeonProRender uber material; every parameter can be a constant, an attribute or a sampler
* samplers: `image1D`, `image2D`, `image3D` (emulated with a 2D atlas) and `transform`
* the `primitive` sampler, `clampToBorder`, `thickness`, attenuation, iridescence and `clearcoatNormal` are not supported
### Lights
* `directional`, `point`, `spot`, `quad`, `hdri` and `ring` (approximated by a disk light), with the units of the ANARI specification
* a visible `hdri` light replaces the renderer background; only one `hdri` light per world is used
### Volumes
* `structuredRegular` spatial fields with `transferFunction1D` volumes
* other spatial fields (`unstructured`, ...) are not supported
### Features which are not implemented
* motion blur and stereo cameras
* frame channels other than color and depth
## Dependencies
* [ANARI SDK](https://github.com/KhronosGroup/ANARI-SDK) 0.17, installed before the device is built
* [RadeonProRender SDK](https://github.com/GPUOpen-LibrariesAndSDKs/RadeonProRenderSDK) 3.1.7.1 with its `hipbin` submodule (the precompiled kernels). CMake downloads it, unless `RPR_SDK_ROOT` points to a checkout.
* on NVIDIA GPUs: the CUDA 13 runtime (`cudart64_13.dll`), which Northstar loads. Set `RPR_CUDA_RUNTIME_LIBRARY` to install it next to the device.
## Building the device
The repository uses CMake to build the device, in a directory separate from the source directory.
### Windows
Visual Studio 2022 is used:
```shell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
  -Danari_DIR=path/to/anari/lib/cmake/anari-0.17.0 ^
  -DRPR_SDK_ROOT=path/to/RadeonProRenderSDK ^
  -DRPR_CUDA_RUNTIME_LIBRARY=path/to/cudart64_13.dll ^
  -DCMAKE_INSTALL_PREFIX=path/to/install
cmake --build build --config Release
cmake --install build --config Release
```
The install step copies the device, `RadeonProRender64.dll`, `Northstar64.dll`, the CUDA
runtime and the kernels (548 MB, `-DRPR_ANARI_INSTALL_KERNELS=OFF` skips them) into
`bin`. The device loads them from its own directory.
### UNIX-like systems
```shell
cmake -S . -B build -Danari_DIR=path/to/anari/lib/cmake/anari-0.17.0 -DCMAKE_INSTALL_PREFIX=path/to/install
cmake --build build -t install
```
The directory with the installed device has to be in `LD_LIBRARY_PATH`.
