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
#include "Image.h"

namespace anari::rpr {

// RPR materials have only lookups into 2D images. The slices of the 3D image are the tiles of a 2D image,
// the material nodes of the sampler interpolate between the two tiles next to the third coordinate.
struct Image3D : public Image
{
 public:
  explicit Image3D(RPRGlobalState *s);
  void commitParameters() override;
  void finalize() override;

  MaterialValue generateMaterial(MaterialGraph &graph) const override;

 private:
  helium::ChangeObserverPtr<Array3D> m_data;
  rpr_image_wrap_type                m_wrap_mode_w{RPR_IMAGE_WRAP_TYPE_CLAMP_TO_EDGE};
  float3                             m_size{1.f}; // size of the 3D image
  float2                             m_tiles{1.f}; // number of tiles of the 2D image along its axes
};

} // namespace anari::rpr
