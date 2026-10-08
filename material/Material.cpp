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

#include "Material.h"
#include "MatteMaterial.h"
#include "PhysicallyBasedMaterial.h"

namespace anari {
namespace rpr {

// MaterialParameter definitions //////////////////////////////////////////////

MaterialParameter::MaterialParameter(Object *material) : m_material(material), m_sampler(material) {}

void MaterialParameter::commit(const char *name, const float4 &defaultValue)
{
  m_value     = defaultValue;
  m_attribute = Attribute::NONE;
  m_sampler   = nullptr;

  float  f = 0.f;
  float3 v3(0.f);
  float4 v4(0.f);
  if (m_material->getParam(name, ANARI_FLOAT32, &f))
  {
    m_value = float4(f, f, f, 1.f);
  } else if (m_material->getParam(name, ANARI_FLOAT32_VEC3, &v3))
  {
    m_value = float4(v3, 1.f);
  } else if (m_material->getParam(name, ANARI_FLOAT32_VEC4, &v4))
  {
    m_value = v4;
  } else if (m_material->hasParam(name, ANARI_STRING))
  {
    const std::string attribute = m_material->getParamString(name, "");
    m_attribute                 = attributeFromString(attribute);
    if (m_attribute == Attribute::NONE)
    {
      m_material->reportMessage(ANARI_SEVERITY_WARNING,
          "unsupported attribute '%s' for material parameter '%s', the default value is used",
          attribute.c_str(), name);
    }
  } else if (m_material->hasParam(name, ANARI_SAMPLER))
  {
    // The sampler can be a placeholder for an unsupported subtype. Whether a supported sampler is valid
    // is not known before it is finalized, so this is checked when the material nodes are generated.
    m_sampler = dynamic_cast<Sampler *>(m_material->getParamObject<helium::BaseObject>(name));
    if (!m_sampler)
    {
      m_material->reportMessage(ANARI_SEVERITY_WARNING,
          "unsupported sampler for material parameter '%s', the default value is used", name);
    }
  } else if (m_material->hasParam(name))
  {
    m_material->reportMessage(
        ANARI_SEVERITY_WARNING, "unsupported type of material parameter '%s', the default value is used", name);
  }
}

MaterialValue MaterialParameter::generateMaterial(MaterialGraph &graph) const
{
  if (m_sampler && m_sampler->isValid())
    return m_sampler->generateMaterial(graph);
  if (m_attribute != Attribute::NONE)
    return graph.attribute(m_attribute);
  return MaterialValue(m_value);
}

bool MaterialParameter::isConstant() const
{
  return !(m_sampler && m_sampler->isValid()) && m_attribute == Attribute::NONE;
}

bool MaterialParameter::isConstant(float value) const
{
  return isConstant() && m_value.x == value && m_value.y == value && m_value.z == value;
}

// Material definitions ///////////////////////////////////////////////////////

Material::Material(RPRGlobalState *s) : Object(ANARI_MATERIAL, s) {}

Material *Material::createInstance(std::string_view type, RPRGlobalState *s)
{
  if (type == "matte")
  {
    return new MatteMaterial(s);
  }
  if (type == "physicallyBased")
  {
    return new PhysicallyBasedMaterial(s);
  }
  return (Material *)new UnknownObject(ANARI_MATERIAL, type, s);
}

void Material::commitParameters()
{
  m_alpha_mode   = alphaModeFromString(getParamString("alphaMode", "opaque"));
  m_alpha_cutoff = getParam<float>("alphaCutoff", 0.5f);
}

void Material::finalize()
{
  // the nodes are generated again when the scene is updated
  m_graphs.clear();
}

void Material::markFinalized()
{
  Object::markFinalized();
  markSceneChanged();
}

rpr_material_node Material::getMaterial(const AttributeContext &context)
{
  auto &graph = m_graphs[context];
  if (!graph)
    graph = createMaterial(context);
  return graph->root;
}

std::unique_ptr<MaterialGraph> Material::createMaterial(const AttributeContext &context) const
{
  auto graph = std::make_unique<MaterialGraph>(deviceState(), context);
  generateMaterial(*graph);
  return graph;
}

MaterialValue Material::generateTransparency(
    MaterialGraph &graph, const MaterialValue &opacity, const MaterialValue &color) const
{
  if (m_alpha_mode == AlphaMode::OPAQUE)
    return MaterialValue(0.f);

  // the alpha channel of the color is part of the opacity
  MaterialValue alpha = graph.mul(graph.select(opacity, 0), graph.select(color, 3));
  if (m_alpha_mode == AlphaMode::MASK)
    alpha = graph.arithmetic(RPR_MATERIAL_NODE_OP_GREATER_OR_EQUAL, alpha, MaterialValue(m_alpha_cutoff));

  return graph.sub(MaterialValue(1.f), alpha);
}

} // namespace rpr

ANARI_TYPEFOR_DEFINITION(rpr::Material *);

} // namespace anari
