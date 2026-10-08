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

#include <map>
#include "../../rpr_common.h"
#include "../SceneObject.h"
#include "MeshData.h"

namespace anari {
namespace rpr {

struct Geometry : public SceneObject
{
  explicit Geometry(RPRGlobalState *s);
  ~Geometry() override;

  static Geometry *createInstance(std::string_view type, RPRGlobalState *s);

  void commitParameters() override;
  bool isValid() const override;

  // The RPR mesh of the geometry. It is never part of a scene: surfaces create instances of it.
  rpr_shape getBaseShape() const;

  // attribute is stored per vertex as primvar of the base shape, the key is the attribute slot
  bool hasAttribute(int slot) const;

  // Geometries of many equal primitives (spheres) are not one big mesh: the base shape is a single
  // primitive and the geometry is a set of instances of it. This is much faster to build for RPR and
  // needs less memory.
  bool                     isInstanced() const;
  const std::vector<mat4> &instanceTransforms() const;
  // values of an attribute per instance (the index is the object ID of the instance), can be null
  rpr_buffer attributeBuffer(int slot) const;
  // constant values of the attributes of the whole geometry
  const UniformAttributeSet &uniformAttributes() const;

 protected:
  // replaces the base shape by a new mesh
  void setMesh(const MeshView &mesh);
  void clearMesh();

  // makes the geometry a set of instances of the base shape, with optional attribute values per instance
  void setInstances(std::vector<mat4> &&transforms,
      const std::array<std::vector<float4>, NUM_ATTRIBUTES> &attributes, const box3 &bounds);

  // returns the array if it is a valid attribute array with at least 'size' elements
  const Array1D *checkAttributeArray(const Array1D *array, size_t size, const char *level, int slot) const;

  rpr_shape                        m_base_shape{};
  std::array<bool, NUM_ATTRIBUTES> m_has_attribute{};

  std::vector<mat4>                      m_instance_transforms;
  std::array<rpr_buffer, NUM_ATTRIBUTES> m_attribute_buffers{};
  UniformAttributeSet              m_uniform_attributes;

  std::array<helium::ChangeObserverPtr<Array1D>, NUM_ATTRIBUTES> m_vertex_attributes;
  std::array<helium::ChangeObserverPtr<Array1D>, NUM_ATTRIBUTES> m_primitive_attributes;
};

} // namespace rpr

ANARI_TYPEFOR_SPECIALIZATION(rpr::Geometry *, ANARI_GEOMETRY)

} // namespace anari
