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

#include "../Light.h"

namespace anari::rpr {

// Rectangular area light: a mesh with an emissive material
struct QuadLight : public Light
{
  explicit QuadLight(RPRGlobalState *s);
  ~QuadLight() override;

  void commitParameters() override;
  void finalize() override;
  void attachToScene(rpr_scene scene, const mat4 &transform, SceneItems &items) override;

 private:
  void clear();

  rpr_material_node m_material{};

  float3      m_position{0.f};
  float3      m_edge1{1.f, 0.f, 0.f};
  float3      m_edge2{0.f, 1.f, 0.f};
  float       m_radiance{1.f}; // W/sr/m^2
  std::string m_side;
};

} // namespace anari::rpr
