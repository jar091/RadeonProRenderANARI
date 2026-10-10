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

#pragma once

#include "RPRMath.h"
#include "rpr_common.h"
// helium
#include "helium/BaseGlobalDeviceState.h"
#include "helium/TaskQueue.h"
// std
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace anari::rpr {

// Order in which RPR objects of one garbage collection are deleted
enum class DeleteOrder
{
  SCENE_ITEM, // instances, lights and volumes which can be attached to a scene
  SHAPE, // base meshes
  MATERIAL, // material nodes
  DATA, // images, buffers, grids
  FRAMEBUFFER,
  SCENE
};

struct RPRGlobalState : public helium::BaseGlobalDeviceState
{
  explicit RPRGlobalState(ANARIDevice d);

  // RPR handles. They are created on first use (see RPRDevice::initDevice())
  rpr_context         context{nullptr};
  rpr_material_system matsys{nullptr};
  bool                usesGPU{false};
  std::string         computeDeviceName;

  // RPR threw an exception while rendering (for example because the compilation of its kernels failed).
  // The context is not usable any more and RPR terminates the application if its objects are deleted,
  // so they are leaked from then on.
  bool contextFailed{false};

  // the node definitions and the attribute names of MaterialX documents were given to the context
  // (see MaterialXMaterial.cpp)
  bool materialXInitialized{false};

  // The RPR context is not thread safe: every RPR call is done while this mutex is locked. RPR objects are
  // only created and modified when the commit buffer is flushed or a frame is rendered.
  std::recursive_mutex rprMutex;

  // thread which renders the frames
  helium::tasking::TaskQueue taskQueue{8};

  anari::Device anariDevice{nullptr}; // public handle of the device

  struct ObjectUpdates
  {
    helium::TimeStamp lastSceneChange{0}; // an object which is part of the RPR scene was finalized
  } objectUpdates;

  // ANARI objects can be released from every thread at any time, even while a frame is rendered. That is
  // why RPR objects are never deleted directly: they are collected here and deleted by the render thread.
  void deleteLater(void *rprObject, DeleteOrder order);
  void collectGarbage(); // rprMutex has to be locked
  // deletes a RPR object immediately, errors and exceptions of RPR are reported
  void deleteObject(void *rprObject, const char *name) const;

 private:
  std::mutex                                 m_garbageMutex;
  std::vector<std::pair<DeleteOrder, void *>> m_garbage;
};

// Returns the directory of the device library (with trailing separator), empty if it is unknown
std::string deviceLibraryDirectory();

// Loads RadeonProRender core library from the directory of the device library
void loadRPRLibrary();

// Loads a library from the directory of the device library or, if it is not there, from the search path
// of the application. Returns false if the library is not available.
bool loadRuntimeLibrary(const char *name);

// rprCreateContext which survives a crash of the render plugin (it returns an error code then)
rpr_status createContextGuarded(rpr_int *plugins, size_t pluginCount, rpr_creation_flags flags,
    const rpr_context_properties *properties, const char *cachePath, rpr_context *context);

// Returns a writable directory for the cache files of RPR: 'path' or, if it is empty, the directory
// 'cache' next to the device library or in the directory for temporary files.
std::string cacheDirectory(const std::string &path);

} // namespace anari::rpr
