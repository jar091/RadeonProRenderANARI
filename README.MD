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
* path depth, counted like in Cycles (the limits of RadeonProRender are these values plus one, measured with Northstar 3.1.7):
  * `maxBounce`: bounces of a path after the point which the camera sees, 0 is direct light only (`RPR_CONTEXT_MAX_RECURSION`). It takes precedence over `maxRayDepth`, which is the value of RadeonProRender as it is (`maxRayDepth` 8 = `maxBounce` 7). A surface with transparency which a path passes counts as a bounce; there is no separate limit for transparency, and shadow rays pass any number of transparent surfaces
  * `maxDiffuseBounce`: diffuse bounces (`RPR_CONTEXT_MAX_DEPTH_DIFFUSE`), default: the limit of all bounces
  * limits for glossy, transmission and volume bounces are not available: Northstar ignores `RPR_CONTEXT_MAX_DEPTH_GLOSSY`, `_REFRACTION`, `_GLOSSY_REFRACTION` and `_SHADOW` and has no limit for volumes
* `clampIndirect` (`RPR_CONTEXT_RADIANCE_CLAMP`, 0: no clamp) limits every color channel of the light which a path collects after its first bounce; it takes precedence over `radianceClamp`, which is the same value. Direct light and directly visible emission are never clamped (there is no counterpart of the direct clamp of Cycles)
* a change of these parameters restarts the accumulation, the scene is not built again
### Camera
* supported types: `perspective` (with depth of field) and `orthographic`
* `imageRegion` selects the part of the image plane which the frame shows (lens shift, crop, zoom). The perspective camera uses a smaller sensor with the same focal length and the lens shift of RadeonProRender, so direction, perspective and depth of field stay the same; the orthographic camera is resized and moved in its image plane
### Geometry
* `triangle` and `quad` are RadeonProRender meshes
* `sphere` is rendered with instances of one sphere mesh
* `cylinder`, `cone` and `curve` are tessellated
* `isosurface` is extracted from `structuredRegular` fields
### Instances
* `transform` instances, also with an array of transformations (`ANARI_KHR_INSTANCE_TRANSFORM_ARRAY`)
* ray visibility of the surfaces of an instance: the `BOOL` parameters `visibility.camera`, `visibility.diffuse`, `visibility.glossy`, `visibility.transmission`, `visibility.volumeScatter` and `visibility.shadow` (default: true) hide them from camera rays, from rays reflected by diffuse lobes (an emissive surface then casts no light on diffuse surfaces), from specular and glossy reflections, from refracted rays and from shadow rays. They are the visibility flags of RadeonProRender shapes; `visibility.volumeScatter` has no counterpart and only matters when all six are false: then the instance, including its lights and volumes, is not part of the scene. Behind a surface which is transparent without refraction (opacity, or transmission with an index of refraction of 1) the camera flag decides, behind a refracting surface the transmission flag. Lights and volumes of an instance are not affected by single flags
* the RadeonProRender instances of a scene are kept when the scene is updated: a change of a transformation, a material or a visibility flag does not build the meshes again
### Materials and samplers
* `matte` and `physicallyBased` use the RadeonProRender uber material; every parameter can be a constant, an attribute or a sampler
* `materialx` is a MaterialX document which is loaded by the MaterialX loader of RadeonProRender:
  * `source` is the XML text of the document (`sourceType` = `documentInline`, the default) or the path of a `.mtlx` file (`documentFile`); `materialName` selects one `surfacematerial` of an inline document (default: the first one)
  * the images are `image2D` sampler parameters of the material: a parameter with the name of a file of the document (the value of a `filename` input, for example `image1/file`) provides the image of that file; only the image of the sampler is used. The color space attributes of the document are ignored: an image is linear unless its array has an sRGB element type
  * the node definitions of `open_pbr_surface`, `standard_surface` and `UsdPreviewSurface` are compiled into the device (`material/materialx`); they are adapted to the loader, see the comments in the files. Nodes of the standard library which the loader does not know (for example `checkerboard`, `randomfloat`, `blackbody`) are an error
  * an inline document is rewritten before it is loaded, because the loader renders some valid constructs wrong without a message (`material/MaterialXDocument.h`): inputs which are connected through document-level `output` elements, `separate2/3/4` nodes, `mix` and `add` of EDFs, the roughness of BSDF nodes which does not come from a `roughness_anisotropy` node and the `opacity` of `surface` nodes. Only nodes at document level are rewritten, not the content of node graphs
  * `texcoord` nodes read `attribute0` (index 0) to `attribute3` (index 3), `geomcolor` reads `color`; `geompropvalue` nodes can read `attribute0`..`attribute3` and `color` by name. Only attributes which are defined per vertex or per primitive are available
  * opacity (`geometry_opacity` of `open_pbr_surface`, `opacity` of `standard_surface`, `UsdPreviewSurface` and of `surface` nodes) is rendered as transmission without refraction, the alpha channel of the frame stays opaque; `alphaMode` and `alphaCutoff` are ignored
  * a document which cannot be loaded is reported as an error and rendered with a gray diffuse material; the property `valid` of the material is false then
  * the document is loaded again only when `source`, `sourceType`, `materialName` or one of its images changes (about 5-10 ms per document)
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
