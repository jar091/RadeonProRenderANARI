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

#include "../Geometry.h"

namespace anari::rpr {

// Base of the geometries which RPR does not have as primitives: spheres, cylinders, cones and curves.
// They are tessellated to one polygon mesh per geometry, attributes become vertex attributes of the mesh.
struct Primitives : public Geometry
{
 public:
  explicit Primitives(RPRGlobalState *s);
  void commitParameters() override;

 protected:
  // checks 'vertex.position', returns the number of vertices or 0 if the geometry has no valid vertices
  size_t checkVertices(const char *subtype);
  // checks an optional array of the geometry, returns nullptr if it can not be used
  const Array1D *checkArray(
      const Array1D *array, ANARIDataType elementType, size_t minSize, const char *name) const;

  // selects the attribute arrays which are used for 'numVertices' vertices and 'numPrimitives' primitives
  // and enables their slots in the mesh
  void prepareAttributes(MeshData &mesh, size_t numVertices, size_t numPrimitives);
  // values of all attributes for a vertex of a primitive
  AttributeValues attributeValues(size_t vertex, size_t primitive) const;

  // The tessellation of a geometry gets coarser if it has more triangles than this limit: RPR needs
  // several seconds per million triangles to build its acceleration structure.
  static constexpr size_t MAX_TRIANGLES = 2000000;

  helium::ChangeObserverPtr<Array1D> m_vertices;
  helium::ChangeObserverPtr<Array1D> m_indices;

 private:
  std::array<const Array1D *, NUM_ATTRIBUTES> m_used_vertex_attributes{};
  std::array<const Array1D *, NUM_ATTRIBUTES> m_used_primitive_attributes{};
};

} // namespace anari::rpr
