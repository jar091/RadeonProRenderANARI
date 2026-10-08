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

#include "MatteMaterial.h"

namespace anari::rpr {

MatteMaterial::MatteMaterial(RPRGlobalState *s) : Material(s), m_color(this), m_opacity(this) {}

void MatteMaterial::commitParameters()
{
  Material::commitParameters();
  m_color.commit("color", float4(0.8f, 0.8f, 0.8f, 1.f));
  m_opacity.commit("opacity", float4(1.f));
}

void MatteMaterial::generateMaterial(MaterialGraph &graph) const
{
  rpr_material_node material = graph.createNode(RPR_MATERIAL_NODE_UBERV2);
  graph.root                 = material;

  const MaterialValue color = m_color.generateMaterial(graph);

  // only the diffuse layer of the uber material is used
  graph.setInput(material, RPR_MATERIAL_INPUT_UBER_DIFFUSE_COLOR, color);
  graph.setInput(material, RPR_MATERIAL_INPUT_UBER_DIFFUSE_WEIGHT, MaterialValue(1.f));
  graph.setInput(material, RPR_MATERIAL_INPUT_UBER_DIFFUSE_ROUGHNESS, MaterialValue(0.f));
  graph.setInput(material, RPR_MATERIAL_INPUT_UBER_REFLECTION_WEIGHT, MaterialValue(0.f));
  graph.setInput(material, RPR_MATERIAL_INPUT_UBER_REFRACTION_WEIGHT, MaterialValue(0.f));
  graph.setInput(material, RPR_MATERIAL_INPUT_UBER_COATING_WEIGHT, MaterialValue(0.f));
  graph.setInput(material, RPR_MATERIAL_INPUT_UBER_SHEEN_WEIGHT, MaterialValue(0.f));
  graph.setInput(material, RPR_MATERIAL_INPUT_UBER_EMISSION_WEIGHT, MaterialValue(0.f));
  graph.setInput(material, RPR_MATERIAL_INPUT_UBER_SSS_WEIGHT, MaterialValue(0.f));
  graph.setInput(material, RPR_MATERIAL_INPUT_UBER_BACKSCATTER_WEIGHT, MaterialValue(0.f));

  graph.setInput(material, RPR_MATERIAL_INPUT_UBER_TRANSPARENCY,
      generateTransparency(graph, m_opacity.generateMaterial(graph), color));
}

} // namespace anari::rpr
