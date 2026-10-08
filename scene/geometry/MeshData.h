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

#include "attributes/Attribute.h"
// std
#include <vector>

namespace anari::rpr {

using AttributeValues = std::array<float4, NUM_ATTRIBUTES>;

// Description of a polygon mesh for the creation of a RPR shape. The data is not owned.
struct MeshView
{
  const float3  *positions{nullptr};
  size_t         num_vertices{0};
  const float3  *normals{nullptr}; // optional, one per vertex. Faces are flat shaded without normals
  const rpr_int *indices{nullptr}; // 'face_size' indices per face
  size_t         num_faces{0};
  int            face_size{3};
  // optional values of the attribute slots, one per vertex
  std::array<const float4 *, NUM_ATTRIBUTES> attributes{};
};

// Polygon mesh which is built by the device: unshared meshes with attributes per primitive and
// tessellated spheres, cylinders, cones and curves (RPR has only polygon meshes with attributes)
struct MeshData
{
  std::vector<float3>                                positions;
  std::vector<float3>                                normals;
  std::vector<rpr_int>                               indices;
  int                                                face_size{3};
  std::array<std::vector<float4>, NUM_ATTRIBUTES>    attributes;
  // only the values of these attribute slots are stored by the functions which add primitives
  std::array<bool, NUM_ATTRIBUTES> has_attribute{};

  MeshView view() const;

  void reserve(size_t numVertices, size_t numTriangles);

  // sphere as subdivided icosahedron, 'level' is the number of subdivisions (0..4)
  void addSphere(const float3 &center, float radius, int level, const AttributeValues &values);

  // truncated cone between two points with optional caps, cylinders have equal radii
  void addCone(const float3 &p0, float r0, const float3 &p1, float r1, bool cap0, bool cap1, int sides,
      const AttributeValues &values0, const AttributeValues &values1);

  static size_t sphereVertexCount(int level);
  static size_t sphereTriangleCount(int level);

 private:
  void addAttributes(const AttributeValues &values, size_t count);
};

} // namespace anari::rpr
