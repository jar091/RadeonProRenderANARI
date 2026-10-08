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

#include "PhysicallyBasedMaterial.h"

namespace anari::rpr {

PhysicallyBasedMaterial::PhysicallyBasedMaterial(RPRGlobalState *s)
    : Material(s), m_base_color(this), m_opacity(this), m_metallic(this), m_roughness(this), m_normal(this),
      m_emissive(this), m_specular(this), m_specular_color(this), m_clearcoat(this), m_clearcoat_roughness(this),
      m_transmission(this), m_sheen_color(this), m_sheen_roughness(this)
{}

void PhysicallyBasedMaterial::commitParameters()
{
  Material::commitParameters();
  m_base_color.commit("baseColor", float4(1.f, 1.f, 1.f, 1.f));
  m_opacity.commit("opacity", float4(1.f));
  m_metallic.commit("metallic", float4(1.f));
  m_roughness.commit("roughness", float4(1.f));
  m_normal.commit("normal", float4(0.5f, 0.5f, 1.f, 1.f));
  m_emissive.commit("emissive", float4(0.f, 0.f, 0.f, 1.f));
  m_specular.commit("specular", float4(1.f));
  m_specular_color.commit("specularColor", float4(1.f));
  m_clearcoat.commit("clearcoat", float4(0.f, 0.f, 0.f, 1.f));
  m_clearcoat_roughness.commit("clearcoatRoughness", float4(0.f, 0.f, 0.f, 1.f));
  m_transmission.commit("transmission", float4(0.f, 0.f, 0.f, 1.f));
  m_sheen_color.commit("sheenColor", float4(0.f, 0.f, 0.f, 1.f));
  m_sheen_roughness.commit("sheenRoughness", float4(0.f, 0.f, 0.f, 1.f));
  m_ior        = getParam<float>("ior", 1.5f);
  m_has_normal = hasParam("normal", ANARI_SAMPLER);

  static const char *unsupported[] = {"occlusion", "thickness", "attenuationDistance", "attenuationColor",
      "iridescence", "iridescenceIor", "iridescenceThickness", "clearcoatNormal"};
  for (const char *name : unsupported)
  {
    if (hasParam(name))
    {
      reportMessage(ANARI_SEVERITY_INFO, "parameter '%s' of physicallyBased material is not supported", name);
    }
  }
}

void PhysicallyBasedMaterial::generateMaterial(MaterialGraph &graph) const
{
  rpr_material_node material = graph.createNode(RPR_MATERIAL_NODE_UBERV2);
  graph.root                 = material;

  const MaterialValue baseColor = m_base_color.generateMaterial(graph);
  const MaterialValue metallic  = m_metallic.generateMaterial(graph);
  const MaterialValue roughness = m_roughness.generateMaterial(graph);

  // diffuse layer
  graph.setInput(material, RPR_MATERIAL_INPUT_UBER_DIFFUSE_COLOR, baseColor);
  graph.setInput(material, RPR_MATERIAL_INPUT_UBER_DIFFUSE_WEIGHT, MaterialValue(1.f));
  graph.setInput(material, RPR_MATERIAL_INPUT_UBER_DIFFUSE_ROUGHNESS, MaterialValue(0.f));
  graph.setInput(material, RPR_MATERIAL_INPUT_UBER_BACKSCATTER_WEIGHT, MaterialValue(0.f));
  graph.setInput(material, RPR_MATERIAL_INPUT_UBER_SSS_WEIGHT, MaterialValue(0.f));

  // Reflection layer. A material which is not metallic at all is a dielectric with the given index of
  // refraction: its reflection is colored by 'specularColor' and scaled by 'specular'. A dielectric with the
  // index of refraction 1 does not reflect anything.
  // All other materials use the metalness mode of the uber material, which blends between a dielectric
  // and a metal with the base color as reflectance. This mode ignores the index of refraction and its
  // dielectric is more reflective than the one of other renderers, so it is not used for pure dielectrics.
  const bool    isDielectric  = m_metallic.isConstant(0.f);
  const bool    isReflective  = m_ior > 1.001f;
  const float   reflectionIor = std::max(m_ior, 1.001f);
  MaterialValue reflectionWeight(1.f);
  MaterialValue reflectionColor = baseColor;
  if (isDielectric)
  {
    reflectionWeight = isReflective ? graph.select(m_specular.generateMaterial(graph), 0) : MaterialValue(0.f);
    reflectionColor  = m_specular_color.generateMaterial(graph);
  } else if (!isReflective)
  {
    reflectionWeight = graph.select(metallic, 0);
  } else if (!m_specular.isConstant(1.f))
  {
    // 'specular' scales the reflection of the dielectric only: specular + metallic * (1 - specular)
    const MaterialValue specular = graph.select(m_specular.generateMaterial(graph), 0);
    reflectionWeight             = graph.add(
        specular, graph.mul(graph.select(metallic, 0), graph.sub(MaterialValue(1.f), specular)));
  }
  graph.setInput(material, RPR_MATERIAL_INPUT_UBER_REFLECTION_WEIGHT, reflectionWeight);
  // without reflection the material is purely diffuse
  const bool hasReflection = reflectionWeight.isNode() || reflectionWeight.constant.x > 0.f;
  if (hasReflection)
  {
    graph.setInputU(material, RPR_MATERIAL_INPUT_UBER_REFLECTION_MODE,
        isDielectric ? RPR_UBER_MATERIAL_IOR_MODE_PBR : RPR_UBER_MATERIAL_IOR_MODE_METALNESS);
    graph.setInput(material, RPR_MATERIAL_INPUT_UBER_REFLECTION_COLOR, reflectionColor);
    graph.setInput(material, RPR_MATERIAL_INPUT_UBER_REFLECTION_ROUGHNESS, roughness);
    if (!isDielectric)
      graph.setInput(material, RPR_MATERIAL_INPUT_UBER_REFLECTION_METALNESS, metallic);
    graph.setInput(material, RPR_MATERIAL_INPUT_UBER_REFLECTION_IOR, MaterialValue(reflectionIor));
    graph.setInput(material, RPR_MATERIAL_INPUT_UBER_REFLECTION_ANISOTROPY, MaterialValue(0.f));
  }

  // refraction layer
  if (m_transmission.isConstant(0.f))
  {
    graph.setInput(material, RPR_MATERIAL_INPUT_UBER_REFRACTION_WEIGHT, MaterialValue(0.f));
  } else
  {
    graph.setInput(material, RPR_MATERIAL_INPUT_UBER_REFRACTION_WEIGHT, m_transmission.generateMaterial(graph));
    graph.setInput(material, RPR_MATERIAL_INPUT_UBER_REFRACTION_COLOR, baseColor);
    graph.setInput(material, RPR_MATERIAL_INPUT_UBER_REFRACTION_ROUGHNESS, roughness);
    graph.setInput(material, RPR_MATERIAL_INPUT_UBER_REFRACTION_IOR, MaterialValue(m_ior));
    graph.setInputU(material, RPR_MATERIAL_INPUT_UBER_REFRACTION_THIN_SURFACE, 0);
    graph.setInputU(material, RPR_MATERIAL_INPUT_UBER_REFRACTION_CAUSTICS, 1);
  }

  // clearcoat layer
  if (m_clearcoat.isConstant(0.f))
  {
    graph.setInput(material, RPR_MATERIAL_INPUT_UBER_COATING_WEIGHT, MaterialValue(0.f));
  } else
  {
    graph.setInput(material, RPR_MATERIAL_INPUT_UBER_COATING_WEIGHT, m_clearcoat.generateMaterial(graph));
    graph.setInput(material, RPR_MATERIAL_INPUT_UBER_COATING_COLOR, MaterialValue(1.f));
    graph.setInput(
        material, RPR_MATERIAL_INPUT_UBER_COATING_ROUGHNESS, m_clearcoat_roughness.generateMaterial(graph));
    graph.setInputU(material, RPR_MATERIAL_INPUT_UBER_COATING_MODE, RPR_UBER_MATERIAL_IOR_MODE_PBR);
    graph.setInput(material, RPR_MATERIAL_INPUT_UBER_COATING_IOR, MaterialValue(1.5f));
  }

  // sheen layer
  if (m_sheen_color.isConstant(0.f))
  {
    graph.setInput(material, RPR_MATERIAL_INPUT_UBER_SHEEN_WEIGHT, MaterialValue(0.f));
  } else
  {
    graph.setInput(material, RPR_MATERIAL_INPUT_UBER_SHEEN, m_sheen_color.generateMaterial(graph));
    graph.setInput(material, RPR_MATERIAL_INPUT_UBER_SHEEN_TINT, MaterialValue(1.f));
    graph.setInput(material, RPR_MATERIAL_INPUT_UBER_SHEEN_WEIGHT, MaterialValue(1.f));
  }

  // emission
  if (m_emissive.isConstant(0.f))
  {
    graph.setInput(material, RPR_MATERIAL_INPUT_UBER_EMISSION_WEIGHT, MaterialValue(0.f));
  } else
  {
    graph.setInput(material, RPR_MATERIAL_INPUT_UBER_EMISSION_COLOR, m_emissive.generateMaterial(graph));
    graph.setInput(material, RPR_MATERIAL_INPUT_UBER_EMISSION_WEIGHT, MaterialValue(1.f));
    graph.setInputU(
        material, RPR_MATERIAL_INPUT_UBER_EMISSION_MODE, RPR_UBER_MATERIAL_EMISSION_MODE_DOUBLESIDED);
  }

  // normal map
  if (m_has_normal && !m_normal.isConstant())
  {
    rpr_material_node normalMap = graph.createNode(RPR_MATERIAL_NODE_NORMAL_MAP);
    graph.setInput(normalMap, RPR_MATERIAL_INPUT_COLOR, m_normal.generateMaterial(graph));
    graph.setInput(material, RPR_MATERIAL_INPUT_UBER_DIFFUSE_NORMAL, MaterialValue(normalMap));
    if (hasReflection)
      graph.setInput(material, RPR_MATERIAL_INPUT_UBER_REFLECTION_NORMAL, MaterialValue(normalMap));
    if (!m_transmission.isConstant(0.f))
      graph.setInput(material, RPR_MATERIAL_INPUT_UBER_REFRACTION_NORMAL, MaterialValue(normalMap));
  }

  graph.setInput(material, RPR_MATERIAL_INPUT_UBER_TRANSPARENCY,
      generateTransparency(graph, m_opacity.generateMaterial(graph), baseColor));
}

} // namespace anari::rpr
