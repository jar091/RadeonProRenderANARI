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

#include "Cones.h"

namespace anari::rpr {

Cones::Cones(RPRGlobalState *s) : Primitives(s), m_radius(this) {}

void Cones::commitParameters()
{
  Primitives::commitParameters();
  m_radius = getArray1D(this, "vertex.radius");
  m_caps   = getParamString("caps", "none");
}

void Cones::finalize()
{
  clearMesh();

  const size_t numVertices = checkVertices("cone");
  if (numVertices == 0)
    return;

  const float3  *vertices = m_vertices->beginAs<float3>();
  const Array1D *radius   = checkArray(m_radius.get(), ANARI_FLOAT32, numVertices, "vertex.radius");
  const Array1D *indices  = checkArray(m_indices.get(), ANARI_UINT32_VEC2, 0, "primitive.index");
  if (!radius)
  {
    reportMessage(ANARI_SEVERITY_WARNING, "missing 'vertex.radius' on cone geometry");
    return;
  }

  const size_t numPrimitives = indices ? indices->size() : numVertices / 2;
  if (numPrimitives == 0)
    return;

  const bool firstCap  = m_caps == "first" || m_caps == "both";
  const bool secondCap = m_caps == "second" || m_caps == "both";

  // the more cones the coarser their tessellation
  int sides = 32;
  while (sides > 4 && numPrimitives * size_t(sides) * 4 > MAX_TRIANGLES)
    sides /= 2;

  MeshData mesh;
  prepareAttributes(mesh, numVertices, numPrimitives);
  mesh.reserve(numPrimitives * size_t(sides) * 4 + 2, numPrimitives * size_t(sides) * 4);

  for (size_t primitiveNumber = 0; primitiveNumber < numPrimitives; primitiveNumber++)
  {
    const uint2 index = indices ? indices->beginAs<uint2>()[primitiveNumber]
                                : uint2(uint32_t(primitiveNumber * 2), uint32_t(primitiveNumber * 2 + 1));
    if (index.x >= numVertices || index.y >= numVertices)
      continue;
    const float firstRadius  = std::max(radius->beginAs<float>()[index.x], 0.f);
    const float secondRadius = std::max(radius->beginAs<float>()[index.y], 0.f);
    if (!(firstRadius > 0.f) && !(secondRadius > 0.f))
      continue;
    mesh.addCone(vertices[index.x], firstRadius, vertices[index.y], secondRadius, firstCap, secondCap, sides,
        attributeValues(index.x, primitiveNumber), attributeValues(index.y, primitiveNumber));
  }

  setMesh(mesh.view());
}

} // namespace anari::rpr
