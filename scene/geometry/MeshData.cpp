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

#include "MeshData.h"
// std
#include <map>
#include <mutex>
#include <utility>

namespace anari::rpr {

// Unit sphere templates //////////////////////////////////////////////////////

constexpr int MAX_SPHERE_LEVEL = 4;

struct SphereTemplate
{
  std::vector<float3>  vertices; // normalized
  std::vector<rpr_int> indices;
};

static SphereTemplate makeIcosphere(int level)
{
  SphereTemplate s;

  const float t = (1.f + std::sqrt(5.f)) * 0.5f;
  s.vertices    = {{-1, t, 0}, {1, t, 0}, {-1, -t, 0}, {1, -t, 0}, {0, -1, t}, {0, 1, t}, {0, -1, -t}, {0, 1, -t},
         {t, 0, -1}, {t, 0, 1}, {-t, 0, -1}, {-t, 0, 1}};
  for (auto &v : s.vertices)
    v = linalg::normalize(v);
  s.indices = {0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2, 10, 7, 6, 7, 1, 8,
      3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4, 11, 6, 2, 10, 8, 6, 7, 9, 8, 1};

  for (int l = 0; l < level; l++)
  {
    std::map<std::pair<rpr_int, rpr_int>, rpr_int> midpoints;
    auto midpoint = [&](rpr_int a, rpr_int b) {
      const auto key = std::make_pair(std::min(a, b), std::max(a, b));
      const auto it  = midpoints.find(key);
      if (it != midpoints.end())
        return it->second;
      const rpr_int index = rpr_int(s.vertices.size());
      s.vertices.push_back(linalg::normalize(s.vertices[a] + s.vertices[b]));
      midpoints.emplace(key, index);
      return index;
    };

    std::vector<rpr_int> indices;
    indices.reserve(s.indices.size() * 4);
    for (size_t i = 0; i < s.indices.size(); i += 3)
    {
      const rpr_int a = s.indices[i], b = s.indices[i + 1], c = s.indices[i + 2];
      const rpr_int ab = midpoint(a, b), bc = midpoint(b, c), ca = midpoint(c, a);
      const rpr_int triangles[12] = {a, ab, ca, b, bc, ab, c, ca, bc, ab, bc, ca};
      indices.insert(indices.end(), triangles, triangles + 12);
    }
    s.indices = std::move(indices);
  }
  return s;
}

static const SphereTemplate &sphereTemplate(int level)
{
  static SphereTemplate templates[MAX_SPHERE_LEVEL + 1];
  static std::once_flag once;
  std::call_once(once, []() {
    for (int l = 0; l <= MAX_SPHERE_LEVEL; l++)
      templates[l] = makeIcosphere(l);
  });
  return templates[std::max(0, std::min(level, MAX_SPHERE_LEVEL))];
}

// MeshData definitions ///////////////////////////////////////////////////////

MeshView MeshData::view() const
{
  MeshView v;
  v.positions    = positions.data();
  v.num_vertices = positions.size();
  v.normals      = normals.size() == positions.size() ? normals.data() : nullptr;
  v.indices      = indices.data();
  v.num_faces    = indices.size() / face_size;
  v.face_size    = face_size;
  for (int i = 0; i < NUM_ATTRIBUTES; i++)
    v.attributes[i] = attributes[i].size() == positions.size() ? attributes[i].data() : nullptr;
  return v;
}

void MeshData::reserve(size_t numVertices, size_t numTriangles)
{
  positions.reserve(numVertices);
  normals.reserve(numVertices);
  indices.reserve(numTriangles * 3);
  for (int i = 0; i < NUM_ATTRIBUTES; i++)
  {
    if (has_attribute[i])
      attributes[i].reserve(numVertices);
  }
}

size_t MeshData::sphereVertexCount(int level)
{
  return sphereTemplate(level).vertices.size();
}

size_t MeshData::sphereTriangleCount(int level)
{
  return sphereTemplate(level).indices.size() / 3;
}

void MeshData::addAttributes(const AttributeValues &values, size_t count)
{
  for (int i = 0; i < NUM_ATTRIBUTES; i++)
  {
    if (has_attribute[i])
      attributes[i].insert(attributes[i].end(), count, values[i]);
  }
}

void MeshData::addSphere(const float3 &center, float radius, int level, const AttributeValues &values)
{
  const SphereTemplate &sphere = sphereTemplate(level);
  const rpr_int         base   = rpr_int(positions.size());

  for (const float3 &n : sphere.vertices)
  {
    positions.push_back(center + radius * n);
    normals.push_back(n);
  }
  for (rpr_int index : sphere.indices)
    indices.push_back(base + index);
  addAttributes(values, sphere.vertices.size());
}

void MeshData::addCone(const float3 &p0, float r0, const float3 &p1, float r1, bool cap0, bool cap1, int sides,
    const AttributeValues &values0, const AttributeValues &values1)
{
  const float3 d      = p1 - p0;
  const float  length = linalg::length(d);
  if (!(length > 0.f) || sides < 3)
    return;

  const float3 axis = d / length;
  float3       u, v;
  makeBasis(axis, u, v);

  // the normals of the lateral surface are tilted if the radii differ
  const float slope = (r0 - r1) / length;

  const rpr_int base = rpr_int(positions.size());
  for (int i = 0; i < sides; i++)
  {
    const float  phi    = 2.f * pi * float(i) / float(sides);
    const float3 radial = std::cos(phi) * u + std::sin(phi) * v;
    const float3 normal = linalg::normalize(radial + slope * axis);
    positions.push_back(p0 + r0 * radial);
    normals.push_back(normal);
    positions.push_back(p1 + r1 * radial);
    normals.push_back(normal);
  }
  for (int i = 0; i < sides; i++)
  {
    addAttributes(values0, 1);
    addAttributes(values1, 1);
  }
  for (int i = 0; i < sides; i++)
  {
    const rpr_int a0 = base + 2 * i, a1 = a0 + 1;
    const rpr_int b0 = base + 2 * ((i + 1) % sides), b1 = b0 + 1;
    const rpr_int triangles[6] = {a0, b0, b1, a0, b1, a1};
    indices.insert(indices.end(), triangles, triangles + 6);
  }

  auto addCap = [&](const float3 &center, float radius, const float3 &normal, const AttributeValues &values,
                    bool flip) {
    if (!(radius > 0.f))
      return;
    const rpr_int capBase = rpr_int(positions.size());
    positions.push_back(center);
    normals.push_back(normal);
    for (int i = 0; i < sides; i++)
    {
      const float phi = 2.f * pi * float(i) / float(sides);
      positions.push_back(center + radius * (std::cos(phi) * u + std::sin(phi) * v));
      normals.push_back(normal);
    }
    addAttributes(values, size_t(sides) + 1);
    for (int i = 0; i < sides; i++)
    {
      const rpr_int a = capBase + 1 + i, b = capBase + 1 + (i + 1) % sides;
      const rpr_int triangle[3] = {capBase, flip ? b : a, flip ? a : b};
      indices.insert(indices.end(), triangle, triangle + 3);
    }
  };

  if (cap0)
    addCap(p0, r0, -axis, values0, true);
  if (cap1)
    addCap(p1, r1, axis, values1, false);
}

} // namespace anari::rpr
