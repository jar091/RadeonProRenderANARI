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

#include "Spheres.h"

namespace anari::rpr {

Spheres::Spheres(RPRGlobalState *s) : Primitives(s), m_radius(this) {}

void Spheres::commitParameters()
{
  Primitives::commitParameters();
  m_radius        = getArray1D(this, "vertex.radius");
  m_global_radius = getParam<float>("radius", 1.f);
}

void Spheres::finalize()
{
  clearMesh();

  const size_t numVertices = checkVertices("sphere");
  if (numVertices == 0)
    return;

  const float3  *vertices = m_vertices->beginAs<float3>();
  const Array1D *radius   = checkArray(m_radius.get(), ANARI_FLOAT32, numVertices, "vertex.radius");
  const Array1D *indices  = checkArray(m_indices.get(), ANARI_UINT32, 0, "primitive.index");

  const size_t numPrimitives = indices ? indices->size() : numVertices;
  if (numPrimitives == 0)
    return;

  // The base shape is a unit sphere, every sphere of the geometry is an instance of it. A few spheres
  // are probably big ones, they get a finer tessellation.
  MeshData unitSphere;
  unitSphere.addSphere(float3(0.f), 1.f, numPrimitives <= 16 ? 4 : 3, AttributeValues());
  setMesh(unitSphere.view());

  // attributes per vertex or per primitive are attributes per instance
  MeshData attributeSlots;
  prepareAttributes(attributeSlots, numVertices, numPrimitives);

  std::vector<mat4>                               transforms;
  std::array<std::vector<float4>, NUM_ATTRIBUTES> attributes;
  box3                                            bounds = emptyBox();
  transforms.reserve(numPrimitives);
  for (int slot = 0; slot < NUM_ATTRIBUTES; slot++)
  {
    if (attributeSlots.has_attribute[slot])
      attributes[slot].reserve(numPrimitives);
  }

  for (size_t primitiveNumber = 0; primitiveNumber < numPrimitives; primitiveNumber++)
  {
    const size_t index = indices ? indices->beginAs<uint32_t>()[primitiveNumber] : primitiveNumber;
    if (index >= numVertices)
      continue;
    const float r = radius ? radius->beginAs<float>()[index] : m_global_radius;
    if (!(r > 0.f))
      continue;
    const float3 &center = vertices[index];
    transforms.push_back(
        mat4(float4(r, 0.f, 0.f, 0.f), float4(0.f, r, 0.f, 0.f), float4(0.f, 0.f, r, 0.f), float4(center, 1.f)));
    bounds.extend(center - float3(r));
    bounds.extend(center + float3(r));
    const AttributeValues values = attributeValues(index, primitiveNumber);
    for (int slot = 0; slot < NUM_ATTRIBUTES; slot++)
    {
      if (attributeSlots.has_attribute[slot])
        attributes[slot].push_back(values[slot]);
    }
  }

  if (transforms.empty())
  {
    clearMesh();
    return;
  }
  setInstances(std::move(transforms), attributes, bounds);
}

} // namespace anari::rpr
