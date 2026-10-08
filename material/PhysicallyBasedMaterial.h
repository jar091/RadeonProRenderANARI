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

#include "Material.h"

namespace anari::rpr {

// The physicallyBased material of ANARI (the metallic-roughness model of glTF) implemented with the
// uber material of RPR
struct PhysicallyBasedMaterial : public Material
{
  explicit PhysicallyBasedMaterial(RPRGlobalState *s);

  void commitParameters() override;

 protected:
  void generateMaterial(MaterialGraph &graph) const override;

 private:
  MaterialParameter m_base_color;
  MaterialParameter m_opacity;
  MaterialParameter m_metallic;
  MaterialParameter m_roughness;
  MaterialParameter m_normal;
  MaterialParameter m_emissive;
  MaterialParameter m_specular;
  MaterialParameter m_specular_color;
  MaterialParameter m_clearcoat;
  MaterialParameter m_clearcoat_roughness;
  MaterialParameter m_transmission;
  MaterialParameter m_sheen_color;
  MaterialParameter m_sheen_roughness;
  float             m_ior{1.5f};
  bool              m_has_normal{false};
};

} // namespace anari::rpr
