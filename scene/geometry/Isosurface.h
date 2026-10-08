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

#include "../volume/SpatialField.h"
#include "Geometry.h"

namespace anari::rpr {

// Isosurfaces of a spatial field. RPR has no implicit surfaces: the surface is extracted as triangle mesh
// from the field, which is resampled before if it is coarse.
struct Isosurface : public Geometry
{
  explicit Isosurface(RPRGlobalState *s);
  void commitParameters() override;
  void finalize() override;

 private:
  helium::ChangeObserverPtr<SpatialField> m_field;
  helium::ChangeObserverPtr<Array1D>      m_isovalue_data;
  float                                   m_isovalue{0.f};
  bool                                    m_has_isovalue{false};
};

} // namespace anari::rpr
