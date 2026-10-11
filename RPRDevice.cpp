/**********************************************************************
Copyright 2022 Advanced Micro Devices, Inc
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
********************************************************************/

#include "RPRDevice.h"

#include "Array.h"
#include "camera/Camera.h"
#include "frame/Frame.h"
#include "material/Material.h"
#include "sampler/Sampler.h"
#include "scene/geometry/Geometry.h"
#include "scene/geometry/Surface.h"
#include "scene/Instance.h"
#include "scene/lights/Light.h"
#include "scene/volume/SpatialField.h"
#include "scene/volume/Volume.h"
#include "scene/World.h"

#include "version.h"

// generated query functions
#include "anari_library_rpr_queries.h"

// std
#include <cstdlib>
#include <limits>
#include <mutex>

// Define list of available render plugins
#if defined(_WIN32)
static const char *RPRPluginNorthstar = "Northstar64.dll";
#elif defined(__APPLE__)
static const char *RPRPluginNorthstar = "libNorthstar64.dylib";
#else
static const char *RPRPluginNorthstar = "libNorthstar64.so";
#endif

static const rpr_creation_flags RPRGpuFlags[] = {RPR_CREATION_FLAGS_ENABLE_GPU0, RPR_CREATION_FLAGS_ENABLE_GPU1,
    RPR_CREATION_FLAGS_ENABLE_GPU2, RPR_CREATION_FLAGS_ENABLE_GPU3, RPR_CREATION_FLAGS_ENABLE_GPU4,
    RPR_CREATION_FLAGS_ENABLE_GPU5, RPR_CREATION_FLAGS_ENABLE_GPU6, RPR_CREATION_FLAGS_ENABLE_GPU7,
    RPR_CREATION_FLAGS_ENABLE_GPU8, RPR_CREATION_FLAGS_ENABLE_GPU9, RPR_CREATION_FLAGS_ENABLE_GPU10,
    RPR_CREATION_FLAGS_ENABLE_GPU11, RPR_CREATION_FLAGS_ENABLE_GPU12, RPR_CREATION_FLAGS_ENABLE_GPU13,
    RPR_CREATION_FLAGS_ENABLE_GPU14, RPR_CREATION_FLAGS_ENABLE_GPU15};

namespace anari::rpr {

///////////////////////////////////////////////////////////////////////////////
// Helper functions ///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// Registers the Northstar plugin once per process, returns its ID or -1
static rpr_int registerPlugin()
{
  static rpr_int        pluginID = -1;
  static std::once_flag once;
  std::call_once(once, []() {
    loadRPRLibrary();
    // the plugin is expected next to the device library
    const std::string path = deviceLibraryDirectory() + RPRPluginNorthstar;
    pluginID               = rprRegisterPlugin(path.c_str());
    if (pluginID == -1)
      pluginID = rprRegisterPlugin(RPRPluginNorthstar);
  });
  return pluginID;
}

///////////////////////////////////////////////////////////////////////////////
// RPRDevice definitions //////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// Data Arrays ////////////////////////////////////////////////////////////////

ANARIArray1D RPRDevice::newArray1D(const void *appMemory, ANARIMemoryDeleter deleter, const void *userData,
    ANARIDataType type, uint64_t numItems)
{
  initDevice();

  Array1DMemoryDescriptor md;
  md.appMemory   = appMemory;
  md.deleter     = deleter;
  md.deleterPtr  = userData;
  md.elementType = type;
  md.numItems    = numItems;

  if (anari::isObject(type))
    return (ANARIArray1D) new ObjectArray(deviceState(), md);
  else
    return (ANARIArray1D) new Array1D(deviceState(), md);
}

ANARIArray2D RPRDevice::newArray2D(const void *appMemory, ANARIMemoryDeleter deleter, const void *userData,
    ANARIDataType type, uint64_t numItems1, uint64_t numItems2)
{
  initDevice();

  Array2DMemoryDescriptor md;
  md.appMemory   = appMemory;
  md.deleter     = deleter;
  md.deleterPtr  = userData;
  md.elementType = type;
  md.numItems1   = numItems1;
  md.numItems2   = numItems2;

  return (ANARIArray2D) new Array2D(deviceState(), md);
}

ANARIArray3D RPRDevice::newArray3D(const void *appMemory, ANARIMemoryDeleter deleter, const void *userData,
    ANARIDataType type, uint64_t numItems1, uint64_t numItems2, uint64_t numItems3)
{
  initDevice();

  Array3DMemoryDescriptor md;
  md.appMemory   = appMemory;
  md.deleter     = deleter;
  md.deleterPtr  = userData;
  md.elementType = type;
  md.numItems1   = numItems1;
  md.numItems2   = numItems2;
  md.numItems3   = numItems3;

  return (ANARIArray3D) new Array3D(deviceState(), md);
}

// Renderable Objects /////////////////////////////////////////////////////////

ANARILight RPRDevice::newLight(const char *type)
{
  initDevice();
  return (ANARILight)Light::createInstance(type, deviceState());
}

ANARICamera RPRDevice::newCamera(const char *type)
{
  initDevice();
  return (ANARICamera)Camera::createInstance(type, deviceState());
}

ANARIGeometry RPRDevice::newGeometry(const char *type)
{
  initDevice();
  return (ANARIGeometry)Geometry::createInstance(type, deviceState());
}

ANARISpatialField RPRDevice::newSpatialField(const char *type)
{
  initDevice();
  return (ANARISpatialField)SpatialField::createInstance(type, deviceState());
}

ANARISurface RPRDevice::newSurface()
{
  initDevice();
  return (ANARISurface) new Surface(deviceState());
}

ANARIVolume RPRDevice::newVolume(const char *type)
{
  initDevice();
  return (ANARIVolume)Volume::createInstance(type, deviceState());
}

// Model Meta-Data ////////////////////////////////////////////////////////////

ANARIMaterial RPRDevice::newMaterial(const char *type)
{
  initDevice();
  return (ANARIMaterial)Material::createInstance(type, deviceState());
}

ANARISampler RPRDevice::newSampler(const char *type)
{
  initDevice();
  return (ANARISampler)Sampler::createInstance(type, deviceState());
}

// Instancing /////////////////////////////////////////////////////////////////

ANARIGroup RPRDevice::newGroup()
{
  initDevice();
  return (ANARIGroup) new Group(deviceState());
}

ANARIInstance RPRDevice::newInstance(const char *type)
{
  initDevice();
  const std::string_view subtype = type ? type : "transform";
  if (subtype != "transform")
    return (ANARIInstance) new UnknownObject(ANARI_INSTANCE, subtype, deviceState());
  return (ANARIInstance) new Instance(deviceState());
}

// Top-level Worlds ///////////////////////////////////////////////////////////

ANARIWorld RPRDevice::newWorld()
{
  initDevice();
  return (ANARIWorld) new World(deviceState());
}

// Query functions ////////////////////////////////////////////////////////////

const char **RPRDevice::getObjectSubtypes(ANARIDataType objectType)
{
  return anari_rpr::query_object_types(objectType);
}

const void *RPRDevice::getObjectInfo(
    ANARIDataType objectType, const char *objectSubtype, const char *infoName, ANARIDataType infoType)
{
  return anari_rpr::query_object_info(objectType, objectSubtype, infoName, infoType);
}

const void *RPRDevice::getParameterInfo(ANARIDataType objectType, const char *objectSubtype,
    const char *parameterName, ANARIDataType parameterType, const char *infoName, ANARIDataType infoType)
{
  return anari_rpr::query_param_info(objectType, objectSubtype, parameterName, parameterType, infoName, infoType);
}

// Object + Parameter Lifetime Management /////////////////////////////////////

int RPRDevice::getProperty(
    ANARIObject object, const char *name, ANARIDataType type, void *mem, uint64_t size, uint32_t mask)
{
  if ((void *)object != (void *)this_device() && mask == ANARI_WAIT)
  {
    // waiting for a property flushes the commit buffer, which creates and updates RPR objects
    std::lock_guard<std::recursive_mutex> lock(deviceState()->rprMutex);
    return helium::BaseDevice::getProperty(object, name, type, mem, size, mask);
  }
  return helium::BaseDevice::getProperty(object, name, type, mem, size, mask);
}

// Frame Manipulation /////////////////////////////////////////////////////////

ANARIFrame RPRDevice::newFrame()
{
  initDevice();
  return (ANARIFrame) new Frame(deviceState());
}

// Frame Rendering ////////////////////////////////////////////////////////////

ANARIRenderer RPRDevice::newRenderer(const char *type)
{
  initDevice();
  return (ANARIRenderer)Renderer::createInstance(type, deviceState());
}

// Other RPRDevice definitions ////////////////////////////////////////////////

RPRDevice::RPRDevice(ANARIStatusCallback defaultCallback, const void *userPtr)
    : helium::BaseDevice(defaultCallback, userPtr)
{
  m_state = std::make_unique<RPRGlobalState>(this_device());
  deviceCommitParameters();
}

RPRDevice::RPRDevice(ANARILibrary library) : helium::BaseDevice(library)
{
  m_state = std::make_unique<RPRGlobalState>(this_device());
  deviceCommitParameters();
}

RPRDevice::~RPRDevice()
{
  auto &state = *deviceState();

  state.taskQueue.flush();

  std::lock_guard<std::recursive_mutex> lock(state.rprMutex);
  state.commitBuffer.clear();
  state.collectGarbage();

  reportMessage(ANARI_SEVERITY_DEBUG, "destroying rpr device (%p)", this);

  state.deleteObject(state.matsys, "rprObjectDelete(matsys)");
  state.matsys = nullptr;

  rpr_context context = state.context;
  state.context       = nullptr; // Always delete the RPR Context last.
  state.deleteObject(context, "rprObjectDelete(context)");
}

bool RPRDevice::createContext(
    rpr_creation_flags flags, const std::string &kernelsPath, const std::string &cachePath)
{
  auto &state = *deviceState();

  const rpr_int pluginID = registerPlugin();
  if (pluginID == -1)
  {
    reportMessage(ANARI_SEVERITY_ERROR, "failed to register RadeonProRender plugin '%s'", RPRPluginNorthstar);
    return false;
  }
  rpr_int plugins[]   = {pluginID};
  size_t  pluginCount = sizeof(plugins) / sizeof(plugins[0]);

#ifdef __APPLE__
  flags |= RPR_CREATION_FLAGS_ENABLE_METAL;
#endif

  // rpr_context_properties is a list of <property name>, <value> terminated by a null name
  const rpr_context_properties properties[] = {
      // the folder of the precompiled kernels of Northstar (HIP for AMD and CUDA for NVIDIA GPUs)
      (rpr_context_properties)RPR_CONTEXT_PRECOMPILED_BINARY_PATH,
      (rpr_context_properties)kernelsPath.c_str(),
      (rpr_context_properties)0,
  };

  // without cache path RPR writes its cache to the directory 'cache' of the working directory
  rpr_context      context = nullptr;
  const rpr_status status  = createContextGuarded(
      plugins, pluginCount, flags, properties, cachePath.empty() ? nullptr : cachePath.c_str(), &context);
  if (status != RPR_SUCCESS || !context)
  {
    reportMessage(ANARI_SEVERITY_WARNING, "failed to create RadeonProRender context with flags 0x%x (error %d)",
        unsigned(flags), int(status));
    return false;
  }

  state.context = context;

  // Set active plugin.
  CHECK(rprContextSetActivePlugin(state.context, plugins[0]))
  CHECK(rprContextSetParameterByKey1u(state.context, RPR_CONTEXT_Y_FLIP, 0))
  // textures are converted to linear colors by the device
  CHECK(rprContextSetParameterByKey1f(state.context, RPR_CONTEXT_TEXTURE_GAMMA, 1.f))
  CHECK(rprContextSetParameterByKey1f(state.context, RPR_CONTEXT_DISPLAY_GAMMA, 1.f))
  // rough reflections do not lose the energy of the multiple scattering between the microfacets
  CHECK(rprContextSetParameterByKey1u(state.context, RPR_CONTEXT_REFLECTION_ENERGY_COMPENSATION_ENABLED, 1))

  CHECK(rprContextCreateMaterialSystem(state.context, 0, &state.matsys))
  if (!state.matsys)
  {
    rprObjectDelete(state.context);
    state.context = nullptr;
    return false;
  }
  return true;
}

void RPRDevice::initDevice()
{
  if (m_initialized)
    return;

  auto &state = *deviceState();

  std::lock_guard<std::recursive_mutex> lock(state.rprMutex);
  if (m_initialized)
    return;
  m_initialized = true;

  reportMessage(ANARI_SEVERITY_DEBUG, "initializing rpr device (%p)", this);

  state.anariDevice = (anari::Device)this_device();

  // environment variables override the parameters of the device
  std::string computeDevice = m_compute_device;
  if (const char *env = std::getenv("ANARI_RPR_COMPUTE_DEVICE"))
    computeDevice = env;
  std::string kernelsPath = m_kernels_path;
  if (const char *env = std::getenv("ANARI_RPR_KERNELS_PATH"))
    kernelsPath = env;
  if (kernelsPath.empty())
    kernelsPath = deviceLibraryDirectory() + "hipbin";
  std::string cachePath = m_cache_path;
  if (const char *env = std::getenv("ANARI_RPR_CACHE_PATH"))
    cachePath = env;
  cachePath = cacheDirectory(cachePath);

  const rpr_creation_flags gpuFlag = RPRGpuFlags[std::clamp(m_gpu_index, 0, 15)];
  rpr_creation_flags       flags   = gpuFlag;
  if (computeDevice == "cpu")
    flags = RPR_CREATION_FLAGS_ENABLE_CPU;
  else if (computeDevice == "gpu+cpu" || computeDevice == "cpu+gpu")
    flags = gpuFlag | RPR_CREATION_FLAGS_ENABLE_CPU;
  else if (computeDevice == "opencl")
    flags = gpuFlag | RPR_CREATION_FLAGS_ENABLE_OPENCL;
  else if (computeDevice != "gpu")
    reportMessage(ANARI_SEVERITY_WARNING, "unknown compute device '%s', using 'gpu'", computeDevice.c_str());

  const bool wantsGPU   = (flags & gpuFlag) != 0;
  bool       hasContext = false;

  if (wantsGPU && !(flags & RPR_CREATION_FLAGS_ENABLE_OPENCL))
  {
    // The default GPU backend of Northstar is HIP with precompiled kernels. It needs the HIP runtime of
    // the AMD driver or, for NVIDIA GPUs, the runtime of the CUDA version Northstar was built with.
    bool hasRuntime = true;
#ifdef _WIN32
    static const char *runtimes[] = {
        "cudart64_13.dll", "amdhip64_8.dll", "amdhip64_7.dll", "amdhip64_6.dll", "amdhip64.dll"};
    hasRuntime = false;
    for (const char *runtime : runtimes)
    {
      if (loadRuntimeLibrary(runtime))
      {
        reportMessage(ANARI_SEVERITY_DEBUG, "found GPU runtime library '%s'", runtime);
        hasRuntime = true;
        break;
      }
    }
    if (!hasRuntime)
    {
      reportMessage(ANARI_SEVERITY_WARNING,
          "neither the CUDA runtime 'cudart64_13.dll' nor the HIP runtime 'amdhip64*.dll' was found, which "
          "the HIP/CUDA backend of RadeonProRender needs: copy the library next to the device library");
    }
#endif
    hasContext = hasRuntime && createContext(flags, kernelsPath, cachePath);
  } else
  {
    hasContext = createContext(flags, kernelsPath, cachePath);
  }

  // The OpenCL backend is not used as fallback: it compiles its kernels when the first frame is
  // rendered, which takes minutes and fails with some drivers. It has to be selected explicitly.
  if (!hasContext && flags != RPR_CREATION_FLAGS_ENABLE_CPU)
  {
    reportMessage(ANARI_SEVERITY_WARNING, "GPU rendering is not available, falling back to CPU rendering");
    flags = RPR_CREATION_FLAGS_ENABLE_CPU;
    createContext(flags, kernelsPath, cachePath);
  }

  if (!state.context)
  {
    reportMessage(ANARI_SEVERITY_FATAL_ERROR, "failed to create RadeonProRender context");
    return;
  }

  state.usesGPU = (flags & gpuFlag) != 0;

  char name[1024] = {0};
  if (state.usesGPU)
  {
    static const rpr_context_info gpuNames[] = {RPR_CONTEXT_GPU0_NAME, RPR_CONTEXT_GPU1_NAME,
        RPR_CONTEXT_GPU2_NAME, RPR_CONTEXT_GPU3_NAME, RPR_CONTEXT_GPU4_NAME, RPR_CONTEXT_GPU5_NAME,
        RPR_CONTEXT_GPU6_NAME, RPR_CONTEXT_GPU7_NAME, RPR_CONTEXT_GPU8_NAME, RPR_CONTEXT_GPU9_NAME,
        RPR_CONTEXT_GPU10_NAME, RPR_CONTEXT_GPU11_NAME, RPR_CONTEXT_GPU12_NAME, RPR_CONTEXT_GPU13_NAME,
        RPR_CONTEXT_GPU14_NAME, RPR_CONTEXT_GPU15_NAME};
    rprContextGetInfo(state.context, gpuNames[std::clamp(m_gpu_index, 0, 15)], sizeof(name), name, nullptr);
    state.computeDeviceName = name;
#if defined(__linux__)
    // Northstar creates its context without a GPU when it has no precompiled kernels for the GPU (the
    // CUDA kernels of RadeonProRender SDK 3.1.7 are built for sm_75 to sm_90, so there are none for
    // Blackwell): the GPU has no name then and every frame is black. Fall back to the CPU, as for a
    // context which could not be created.
    if (name[0] == 0 && !(flags & (RPR_CREATION_FLAGS_ENABLE_CPU | RPR_CREATION_FLAGS_ENABLE_OPENCL)))
    {
      reportMessage(ANARI_SEVERITY_WARNING,
          "the RadeonProRender context has no GPU (no precompiled kernels for this GPU?), falling back "
          "to CPU rendering");
      state.deleteObject(state.matsys, "rprObjectDelete(matsys)");
      state.matsys           = nullptr;
      rpr_context gpuContext = state.context;
      state.context          = nullptr;
      state.deleteObject(gpuContext, "rprObjectDelete(context)");
      flags = RPR_CREATION_FLAGS_ENABLE_CPU;
      createContext(flags, kernelsPath, cachePath);
      if (!state.context)
      {
        reportMessage(ANARI_SEVERITY_FATAL_ERROR, "failed to create RadeonProRender context");
        return;
      }
      state.usesGPU = false;
    }
#endif
  }
  if (flags & RPR_CREATION_FLAGS_ENABLE_CPU)
  {
    name[0] = 0;
    rprContextGetInfo(state.context, RPR_CONTEXT_CPU_NAME, sizeof(name), name, nullptr);
    if (!state.computeDeviceName.empty())
      state.computeDeviceName += " + ";
    state.computeDeviceName += name;
  }
  reportMessage(ANARI_SEVERITY_INFO,
      "RadeonProRender %d.%d.%d (Northstar) renders on %s%s (kernels: %s, cache: %s)", RPR_VERSION_MAJOR,
      RPR_VERSION_MINOR, RPR_VERSION_REVISION, state.computeDeviceName.c_str(),
      !state.usesGPU ? "" : (flags & RPR_CREATION_FLAGS_ENABLE_OPENCL) ? " with OpenCL" : " with HIP/CUDA",
      kernelsPath.c_str(), cachePath.c_str());
}

void RPRDevice::deviceCommitParameters()
{
  helium::BaseDevice::deviceCommitParameters();

  m_compute_device = getParamString("computeDevice", "gpu");
  m_gpu_index      = getParam<int>("gpuIndex", 0);
  m_kernels_path   = getParamString("kernelsPath", "");
  m_cache_path     = getParamString("cachePath", "");

  // parameters of older versions of the device: index 0 is the CPU, the GPUs start with index 1
  uint32_t legacyDevice = 0;
  if (getParam("RPRDevice", ANARI_UINT32, &legacyDevice))
  {
    m_compute_device = legacyDevice == 0 ? "cpu" : "gpu";
    m_gpu_index      = legacyDevice == 0 ? 0 : int(legacyDevice) - 1;
  }

  if (m_initialized)
  {
    reportMessage(ANARI_SEVERITY_INFO,
        "the compute device can not be changed after the first object of the device was created");
  }
}

#define RPR_STRINGIFY(s) RPR_STRINGIFY2(s)
#define RPR_STRINGIFY2(s) #s
#define RPR_ANARI_VERSION_STRING                                                                             \
  "rpr " RPR_STRINGIFY(RPR_ANARI_VERSION_MAJOR) "." RPR_STRINGIFY(RPR_ANARI_VERSION_MINOR) "." RPR_STRINGIFY(  \
      RPR_ANARI_VERSION_PATCH)

int RPRDevice::deviceGetProperty(const char *name, ANARIDataType type, void *mem, uint64_t size, uint32_t mask)
{
  static const std::string version_name = RPR_ANARI_VERSION_STRING;
  std::string_view         prop         = name;
  if (prop == "extension" && type == ANARI_STRING_LIST)
  {
    helium::writeToVoidP(mem, anari_rpr::query_extensions());
    return 1;
  } else if (prop == "version" && type == ANARI_INT32)
  {
    int version = RPR_ANARI_VERSION_MAJOR * 10000 + RPR_ANARI_VERSION_MINOR * 100 + RPR_ANARI_VERSION_PATCH;
    helium::writeToVoidP(mem, version);
    return 1;
  } else if (prop == "version.major" && type == ANARI_INT32)
  {
    helium::writeToVoidP(mem, int(RPR_ANARI_VERSION_MAJOR));
    return 1;
  } else if (prop == "version.minor" && type == ANARI_INT32)
  {
    helium::writeToVoidP(mem, int(RPR_ANARI_VERSION_MINOR));
    return 1;
  } else if (prop == "version.patch" && type == ANARI_INT32)
  {
    helium::writeToVoidP(mem, int(RPR_ANARI_VERSION_PATCH));
    return 1;
  } else if (prop == "version.name.size" && type == ANARI_UINT64)
  {
    helium::writeToVoidP(mem, uint64_t(version_name.size() + 1));
    return 1;
  } else if (prop == "version.name" && type == ANARI_STRING)
  {
    std::memset(mem, 0, size);
    std::memcpy(mem, version_name.data(), std::min(uint64_t(version_name.size()), size - 1));
    return 1;
  } else if (prop == "anariVersion.major" && type == ANARI_INT32)
  {
    helium::writeToVoidP(mem, 1);
    return 1;
  } else if (prop == "anariVersion.minor" && type == ANARI_INT32)
  {
    helium::writeToVoidP(mem, 0);
    return 1;
  } else if (prop == "geometryMaxIndex" && type == ANARI_UINT64)
  {
    helium::writeToVoidP(mem, uint64_t(std::numeric_limits<rpr_int>::max()));
    return 1;
  }
  return 0;
}

RPRGlobalState *RPRDevice::deviceState() const
{
  return (RPRGlobalState *)helium::BaseDevice::m_state.get();
}

} // namespace anari::rpr
