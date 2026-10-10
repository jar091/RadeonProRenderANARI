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

#include "../Object.h"
#include "../rpr_common.h"
#include "../sampler/Sampler.h"
#include "MaterialGraph.h"
// std
#include <map>
#include <memory>

namespace anari {
namespace rpr {

// Parameter of a material: a constant value, the name of a geometry attribute or a sampler
struct MaterialParameter
{
  explicit MaterialParameter(Object *material);

  // reads the parameter 'name' of the material
  void commit(const char *name, const float4 &defaultValue);

  MaterialValue generateMaterial(MaterialGraph &graph) const;

  bool isConstant() const;
  bool isConstant(float value) const; // all color components are equal to 'value'

 private:
  Object                            *m_material{};
  float4                             m_value{0.f, 0.f, 0.f, 1.f};
  Attribute                          m_attribute{Attribute::NONE};
  helium::ChangeObserverPtr<Sampler> m_sampler;
};

struct Material : public Object
{
  explicit Material(RPRGlobalState *s);
  static Material *createInstance(std::string_view type, RPRGlobalState *s);

  void commitParameters() override;
  void finalize() override;
  void markFinalized() override;

  // Returns the nodes of the material for shapes with the given attributes. They are owned by the
  // material, they are valid until the material is finalized again.
  const MaterialGraph &getMaterial(const AttributeContext &context);

  // Creates the nodes of the material for shapes with the given attributes, they are owned by the caller.
  std::unique_ptr<MaterialGraph> createMaterial(const AttributeContext &context) const;

 protected:
  virtual void generateMaterial(MaterialGraph &graph) const = 0;

  // transparency of the uber material for an opacity value, depending on the alpha mode
  MaterialValue generateTransparency(
      MaterialGraph &graph, const MaterialValue &opacity, const MaterialValue &color) const;

  AlphaMode m_alpha_mode{AlphaMode::OPAQUE};
  float     m_alpha_cutoff{0.5f};

 private:
  std::map<AttributeContext, std::unique_ptr<MaterialGraph>> m_graphs;
};

} // namespace rpr

ANARI_TYPEFOR_SPECIALIZATION(rpr::Material *, ANARI_MATERIAL)

} // namespace anari
