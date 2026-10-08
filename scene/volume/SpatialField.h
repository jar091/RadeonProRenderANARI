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

#include "../SceneObject.h"
// std
#include <vector>

namespace anari {
namespace rpr {

// Scalar field which is sampled on a regular grid
struct SpatialField : public SceneObject
{
 public:
  explicit SpatialField(RPRGlobalState *s);
  static SpatialField *createInstance(std::string_view type, RPRGlobalState *s);

  // number of samples of the grid along the axes
  virtual uint3 size() const = 0;
  // sample of the grid, the index is x + size.x * (y + size.y * z)
  virtual float value(size_t index) const = 0;
  // position of the first sample and distances between the samples
  virtual float3 origin() const  = 0;
  virtual float3 spacing() const = 0;

  // RPR does not interpolate between the samples of a grid, so coarse fields are resampled with trilinear
  // interpolation. This is the number of samples per sample of the field along every axis which results
  // in a grid with about 'targetSize' samples along the longest axis.
  static uint32_t resamplingFactor(const uint3 &size, uint32_t targetSize);
  // Samples of the field on a grid with 'factor' samples per sample of the field and the same extent.
  std::vector<float> resample(uint32_t factor, uint3 &resampledSize) const;
};

} // namespace rpr

ANARI_TYPEFOR_SPECIALIZATION(rpr::SpatialField *, ANARI_SPATIAL_FIELD)

} // namespace anari
