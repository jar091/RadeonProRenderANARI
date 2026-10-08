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

#include "Quad.h"
#include "Triangles.h"
// std
#include <numeric>

namespace anari::rpr {

Mesh::Mesh(RPRGlobalState *s, int polySize)
    : Geometry(s), m_poly_size(polySize), m_index(this), m_vertex_position(this), m_vertex_normal(this)
{}

void Mesh::commitParameters()
{
  Geometry::commitParameters();
  m_index           = getArray1D(this, "primitive.index");
  m_vertex_position = getArray1D(this, "vertex.position");
  m_vertex_normal   = getArray1D(this, "vertex.normal");
}

void Mesh::finalize()
{
  clearMesh();

  if (!m_vertex_position || m_vertex_position->elementType() != ANARI_FLOAT32_VEC3)
  {
    reportMessage(ANARI_SEVERITY_WARNING,
        "required 'vertex.position' data array is missed or have incorrect type!");
    return;
  }

  const size_t  numVertices = m_vertex_position->size();
  const float3 *positions   = m_vertex_position->beginAs<float3>();

  // process indices
  const ANARIDataType  indexType = m_poly_size == 3 ? ANARI_UINT32_VEC3 : ANARI_UINT32_VEC4;
  const rpr_int       *index     = nullptr;
  size_t               numFaces  = 0;
  std::vector<rpr_int> defaultIndex;

  if (m_index && m_index->elementType() == indexType)
  {
    numFaces = m_index->size();
    index    = (const rpr_int *)m_index->begin();
    for (size_t i = 0; i < numFaces * m_poly_size; i++)
    {
      if (uint32_t(index[i]) >= numVertices)
      {
        reportMessage(ANARI_SEVERITY_WARNING, "'primitive.index' refers to a vertex which does not exist");
        return;
      }
    }
  } else // 'primitive.index' array is missed. We should use default indices
  {
    if (m_index)
    {
      reportMessage(ANARI_SEVERITY_WARNING, "'primitive.index' has to be an array of %s, it is ignored",
          anari::toString(indexType));
    }
    numFaces = numVertices / m_poly_size;
    defaultIndex.resize(numFaces * m_poly_size);
    std::iota(defaultIndex.begin(), defaultIndex.end(), 0);
    index = defaultIndex.data();
  }

  // process normals
  const float3 *normals = nullptr;
  if (m_vertex_normal)
  {
    if (m_vertex_normal->elementType() == ANARI_FLOAT32_VEC3 && m_vertex_normal->size() >= numVertices)
      normals = m_vertex_normal->beginAs<float3>();
    else
      reportMessage(ANARI_SEVERITY_WARNING, "'vertex.normal' has incorrect type or size, it is ignored");
  }

  // process attributes
  std::array<const Array1D *, NUM_ATTRIBUTES> vertexAttributes{};
  std::array<const Array1D *, NUM_ATTRIBUTES> primitiveAttributes{};
  bool                                        hasPrimitiveAttributes = false;
  for (int slot = 0; slot < NUM_ATTRIBUTES; slot++)
  {
    vertexAttributes[slot] = checkAttributeArray(m_vertex_attributes[slot].get(), numVertices, "vertex", slot);
    if (!vertexAttributes[slot])
    {
      primitiveAttributes[slot] =
          checkAttributeArray(m_primitive_attributes[slot].get(), numFaces, "primitive", slot);
      hasPrimitiveAttributes |= primitiveAttributes[slot] != nullptr;
    }
  }

  if (!hasPrimitiveAttributes)
  {
    MeshView mesh;
    mesh.positions    = positions;
    mesh.num_vertices = numVertices;
    mesh.normals      = normals;
    mesh.indices      = index;
    mesh.num_faces    = numFaces;
    mesh.face_size    = m_poly_size;

    std::array<std::vector<float4>, NUM_ATTRIBUTES> converted;
    for (int slot = 0; slot < NUM_ATTRIBUTES; slot++)
    {
      const Array1D *array = vertexAttributes[slot];
      if (!array)
        continue;
      if (array->elementType() == ANARI_FLOAT32_VEC4)
      {
        mesh.attributes[slot] = array->beginAs<float4>();
        continue;
      }
      converted[slot].resize(numVertices);
      for (size_t i = 0; i < numVertices; i++)
        converted[slot][i] = readAttributeValue(array, i);
      mesh.attributes[slot] = converted[slot].data();
    }
    setMesh(mesh);
    return;
  }

  // RPR interpolates all primvars of a mesh the same way: vertices are not shared between the faces
  // if there are attributes per primitive
  MeshData     data;
  const size_t numCorners = numFaces * m_poly_size;
  data.face_size          = m_poly_size;
  data.positions.resize(numCorners);
  if (normals)
    data.normals.resize(numCorners);
  data.indices.resize(numCorners);
  std::iota(data.indices.begin(), data.indices.end(), 0);
  for (size_t i = 0; i < numCorners; i++)
  {
    data.positions[i] = positions[index[i]];
    if (normals)
      data.normals[i] = normals[index[i]];
  }
  for (int slot = 0; slot < NUM_ATTRIBUTES; slot++)
  {
    if (vertexAttributes[slot])
    {
      data.attributes[slot].resize(numCorners);
      for (size_t i = 0; i < numCorners; i++)
        data.attributes[slot][i] = readAttributeValue(vertexAttributes[slot], index[i]);
    } else if (primitiveAttributes[slot])
    {
      data.attributes[slot].resize(numCorners);
      for (size_t i = 0; i < numCorners; i++)
        data.attributes[slot][i] = readAttributeValue(primitiveAttributes[slot], i / m_poly_size);
    }
  }
  setMesh(data.view());
}

} // namespace anari::rpr
