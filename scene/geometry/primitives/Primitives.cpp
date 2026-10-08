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

#include "Primitives.h"

namespace anari::rpr {

Primitives::Primitives(RPRGlobalState *s) : Geometry(s), m_vertices(this), m_indices(this) {}

void Primitives::commitParameters()
{
  Geometry::commitParameters();
  m_vertices = getArray1D(this, "vertex.position");
  m_indices  = getArray1D(this, "primitive.index");
}

size_t Primitives::checkVertices(const char *subtype)
{
  if (!m_vertices || m_vertices->elementType() != ANARI_FLOAT32_VEC3)
  {
    reportMessage(ANARI_SEVERITY_WARNING, "missing 'vertex.position' on %s geometry", subtype);
    return 0;
  }
  return m_vertices->size();
}

const Array1D *Primitives::checkArray(
    const Array1D *array, ANARIDataType elementType, size_t minSize, const char *name) const
{
  if (!array)
    return nullptr;
  if (array->elementType() != elementType || array->size() < minSize)
  {
    reportMessage(ANARI_SEVERITY_WARNING, "'%s' has to be an array of at least %zu %s, it is ignored", name,
        minSize, anari::toString(elementType));
    return nullptr;
  }
  return array;
}

void Primitives::prepareAttributes(MeshData &mesh, size_t numVertices, size_t numPrimitives)
{
  for (int slot = 0; slot < NUM_ATTRIBUTES; slot++)
  {
    m_used_vertex_attributes[slot] =
        checkAttributeArray(m_vertex_attributes[slot].get(), numVertices, "vertex", slot);
    m_used_primitive_attributes[slot] = m_used_vertex_attributes[slot]
        ? nullptr
        : checkAttributeArray(m_primitive_attributes[slot].get(), numPrimitives, "primitive", slot);
    mesh.has_attribute[slot] = m_used_vertex_attributes[slot] || m_used_primitive_attributes[slot];
  }
}

AttributeValues Primitives::attributeValues(size_t vertex, size_t primitive) const
{
  AttributeValues values;
  for (int slot = 0; slot < NUM_ATTRIBUTES; slot++)
  {
    if (m_used_vertex_attributes[slot])
      values[slot] = readAttributeValue(m_used_vertex_attributes[slot], vertex);
    else if (m_used_primitive_attributes[slot])
      values[slot] = readAttributeValue(m_used_primitive_attributes[slot], primitive);
    else
      values[slot] = DEFAULT_ATTRIBUTE_VALUE;
  }
  return values;
}

} // namespace anari::rpr
