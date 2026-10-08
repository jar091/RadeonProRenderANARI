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

#include "SpatialField.h"
#include "Volume.h"

namespace anari::rpr {

// Volume with a transfer function which maps the values of a spatial field to color and opacity.
// It is rendered as RPR volume material: the opacity is the density of the volume, the color its albedo.
struct TransferFunction1DVolume : public Volume
{
 public:
  explicit TransferFunction1DVolume(RPRGlobalState *s);
  ~TransferFunction1DVolume() override;

  void commitParameters() override;
  void finalize() override;
  bool isValid() const override;
  box3 bounds() const override;

  void attachToScene(rpr_scene scene, const mat4 &transform, SceneItems &items) override;

 private:
  void clear();

  // opacity of the transfer function for a normalized value of the field
  float processDensity(float value) const;

  helium::IntrusivePtr<SpatialField> m_field;
  helium::ChangeObserverPtr<Array1D> m_color_data;
  helium::ChangeObserverPtr<Array1D> m_opacity_data;
  float4                             m_color{1.f, 1.f, 1.f, 1.f};
  float                              m_opacity{1.f};
  box1                               m_value_range{0.f, 1.f};
  float                              m_unit_distance{1.f};

  rpr_image         m_color_map{};
  rpr_material_node m_color_material{};
  rpr_material_node m_color_grid_material{};
  rpr_material_node m_density_grid_material{};
  rpr_material_node m_volume_material{};
  rpr_grid          m_color_grid{};
  rpr_grid          m_density_grid{};
  mat4              m_transform{linalg::identity};
};

} // namespace anari::rpr
