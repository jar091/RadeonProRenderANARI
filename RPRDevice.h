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

// rpr
#include "RadeonProRender.h"
// helium
#include "helium/BaseDevice.h"

#include "interface.h"
#include "Object.h"
#include "RPRGlobalState.h"

namespace anari::rpr {

struct RPRDevice : public helium::BaseDevice
{
  /////////////////////////////////////////////////////////////////////////////
  // Main interface to accepting API calls
  /////////////////////////////////////////////////////////////////////////////

  // Data Arrays //////////////////////////////////////////////////////////////

  ANARIArray1D newArray1D(const void *appMemory, ANARIMemoryDeleter deleter, const void *userdata,
      ANARIDataType, uint64_t numItems1) override;

  ANARIArray2D newArray2D(const void *appMemory, ANARIMemoryDeleter deleter, const void *userdata,
      ANARIDataType, uint64_t numItems1, uint64_t numItems2) override;

  ANARIArray3D newArray3D(const void *appMemory, ANARIMemoryDeleter deleter, const void *userdata,
      ANARIDataType, uint64_t numItems1, uint64_t numItems2, uint64_t numItems3) override;

  // Renderable Objects ///////////////////////////////////////////////////////

  ANARILight newLight(const char *type) override;

  ANARICamera newCamera(const char *type) override;

  ANARIGeometry     newGeometry(const char *type) override;
  ANARISpatialField newSpatialField(const char *type) override;

  ANARISurface newSurface() override;
  ANARIVolume  newVolume(const char *type) override;

  // Model Meta-Data //////////////////////////////////////////////////////////

  ANARIMaterial newMaterial(const char *material_type) override;

  ANARISampler newSampler(const char *type) override;

  // Instancing ///////////////////////////////////////////////////////////////

  ANARIGroup newGroup() override;

  ANARIInstance newInstance(const char *type) override;

  // Top-level Worlds /////////////////////////////////////////////////////////

  ANARIWorld newWorld() override;

  // Query functions //////////////////////////////////////////////////////////

  const char **getObjectSubtypes(ANARIDataType objectType) override;
  const void  *getObjectInfo(ANARIDataType objectType, const char *objectSubtype, const char *infoName,
       ANARIDataType infoType) override;
  const void  *getParameterInfo(ANARIDataType objectType, const char *objectSubtype, const char *parameterName,
       ANARIDataType parameterType, const char *infoName, ANARIDataType infoType) override;

  // Object + Parameter Lifetime Management ///////////////////////////////////

  int getProperty(ANARIObject object, const char *name, ANARIDataType type, void *mem, uint64_t size,
      uint32_t mask) override;

  // Frame Manipulation ///////////////////////////////////////////////////////

  ANARIFrame newFrame() override;

  // Frame Rendering //////////////////////////////////////////////////////////

  ANARIRenderer newRenderer(const char *type) override;

  /////////////////////////////////////////////////////////////////////////////
  // Helper/other functions and data members
  /////////////////////////////////////////////////////////////////////////////

  RPRDevice(ANARIStatusCallback defaultCallback, const void *userPtr);
  explicit RPRDevice(ANARILibrary library);
  ~RPRDevice() override;

 private:
  // creates the RPR context when the first object is created
  void initDevice();
  bool createContext(rpr_creation_flags flags, const std::string &kernelsPath, const std::string &cachePath);

  void deviceCommitParameters() override;
  int  deviceGetProperty(
       const char *name, ANARIDataType type, void *mem, uint64_t size, uint32_t mask) override;

  RPRGlobalState *deviceState() const;

  bool        m_initialized{false};
  std::string m_compute_device{"gpu"};
  int         m_gpu_index{0};
  std::string m_kernels_path;
  std::string m_cache_path;
};

} // namespace anari::rpr
