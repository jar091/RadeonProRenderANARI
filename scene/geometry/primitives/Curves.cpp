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

#include "Curves.h"
// std
#include <vector>

namespace anari::rpr {

Curves::Curves(RPRGlobalState *s) : Primitives(s), m_radius(this) {}

void Curves::commitParameters()
{
  Primitives::commitParameters();
  m_radius        = getArray1D(this, "vertex.radius");
  m_global_radius = getParam<float>("radius", 1.f);
}

void Curves::finalize()
{
  clearMesh();

  const size_t numVertices = checkVertices("curve");
  if (numVertices < 2)
    return;

  const float3  *vertices = m_vertices->beginAs<float3>();
  const Array1D *radius   = checkArray(m_radius.get(), ANARI_FLOAT32, numVertices, "vertex.radius");
  const Array1D *indices  = checkArray(m_indices.get(), ANARI_UINT32, 0, "primitive.index");

  // a segment connects the vertex with the given index and the next one
  const size_t numPrimitives = indices ? indices->size() : numVertices - 1;
  if (numPrimitives == 0)
    return;

  // The more segments the coarser their tessellation. The joints of very many segments (hair) are not
  // rounded with spheres.
  int  sides        = 16;
  int  level        = 2;
  auto numTriangles = [&]() {
    return size_t(sides) * 2 + (level >= 0 ? MeshData::sphereTriangleCount(level) : 0);
  };
  while ((sides > 4 || level >= 0) && numPrimitives * numTriangles() > MAX_TRIANGLES)
  {
    sides = std::max(sides / 2, 4);
    level--;
  }

  MeshData mesh;
  prepareAttributes(mesh, numVertices, numPrimitives);

  // primitive (or 0) which defines the attributes of the joint of a vertex, -1 for unused vertices
  std::vector<int64_t> joints(numVertices, -1);

  for (size_t primitiveNumber = 0; primitiveNumber < numPrimitives; primitiveNumber++)
  {
    const size_t first  = indices ? indices->beginAs<uint32_t>()[primitiveNumber] : primitiveNumber;
    const size_t second = first + 1;
    if (second >= numVertices)
      continue;
    const float firstRadius  = radius ? radius->beginAs<float>()[first] : m_global_radius;
    const float secondRadius = radius ? radius->beginAs<float>()[second] : m_global_radius;
    if (!(firstRadius > 0.f) && !(secondRadius > 0.f))
      continue;
    mesh.addCone(vertices[first], std::max(firstRadius, 0.f), vertices[second], std::max(secondRadius, 0.f),
        false, false, sides, attributeValues(first, primitiveNumber), attributeValues(second, primitiveNumber));
    if (joints[first] < 0)
      joints[first] = int64_t(primitiveNumber);
    if (joints[second] < 0)
      joints[second] = int64_t(primitiveNumber);
  }

  for (size_t vertex = 0; vertex < numVertices; vertex++)
  {
    if (joints[vertex] < 0)
      continue;
    const float r = radius ? radius->beginAs<float>()[vertex] : m_global_radius;
    if (r > 0.f && level >= 0)
      mesh.addSphere(vertices[vertex], r, level, attributeValues(vertex, size_t(joints[vertex])));
  }

  setMesh(mesh.view());
}

} // namespace anari::rpr
