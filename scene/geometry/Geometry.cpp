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

#include "Geometry.h"
// specific types
#include "Isosurface.h"
#include "mesh/Quad.h"
#include "mesh/Triangles.h"
#include "primitives/Cones.h"
#include "primitives/Curves.h"
#include "primitives/Cylinders.h"
#include "primitives/Spheres.h"
// std
#include <chrono>

namespace anari {
namespace rpr {

using ArrayPtr = helium::ChangeObserverPtr<Array1D>;

Geometry::Geometry(RPRGlobalState *s)
    : SceneObject(ANARI_GEOMETRY, s),
      m_vertex_attributes{ArrayPtr(this), ArrayPtr(this), ArrayPtr(this), ArrayPtr(this), ArrayPtr(this)},
      m_primitive_attributes{ArrayPtr(this), ArrayPtr(this), ArrayPtr(this), ArrayPtr(this), ArrayPtr(this)}
{}

Geometry *Geometry::createInstance(std::string_view type, RPRGlobalState *s)
{
  if (type == "triangle")
  {
    return new Triangles(s);
  }
  if (type == "quad")
  {
    return new Quad(s);
  }
  if (type == "sphere")
  {
    return new Spheres(s);
  }
  if (type == "cylinder")
  {
    return new Cylinders(s);
  }
  if (type == "cone")
  {
    return new Cones(s);
  }
  if (type == "curve")
  {
    return new Curves(s);
  }
  if (type == "isosurface")
  {
    return new Isosurface(s);
  }
  return (Geometry *)new UnknownObject(ANARI_GEOMETRY, type, s);
}

void Geometry::commitParameters()
{
  for (int slot = 0; slot < NUM_ATTRIBUTES; slot++)
  {
    const std::string name = attributeName(slot);

    m_uniform_attributes[slot].reset();
    float4 value = DEFAULT_ATTRIBUTE_VALUE;
    if (getParam(name, ANARI_FLOAT32_VEC4, &value))
      m_uniform_attributes[slot] = value;

    m_vertex_attributes[slot]    = getArray1D(this, "vertex." + name);
    m_primitive_attributes[slot] = getArray1D(this, "primitive." + name);
  }
}

bool Geometry::isValid() const
{
  return m_base_shape != nullptr;
}

rpr_shape Geometry::getBaseShape() const
{
  return m_base_shape;
}

bool Geometry::hasAttribute(int slot) const
{
  return m_has_attribute[slot];
}

const UniformAttributeSet &Geometry::uniformAttributes() const
{
  return m_uniform_attributes;
}

bool Geometry::isInstanced() const
{
  return !m_instance_transforms.empty();
}

const std::vector<mat4> &Geometry::instanceTransforms() const
{
  return m_instance_transforms;
}

rpr_buffer Geometry::attributeBuffer(int slot) const
{
  return m_attribute_buffers[slot];
}

void Geometry::setInstances(std::vector<mat4> &&transforms,
    const std::array<std::vector<float4>, NUM_ATTRIBUTES> &attributes, const box3 &bounds)
{
  m_instance_transforms = std::move(transforms);
  m_bounds              = bounds;

  for (int slot = 0; slot < NUM_ATTRIBUTES; slot++)
  {
    if (attributes[slot].size() != m_instance_transforms.size() || attributes[slot].empty())
      continue;
    rpr_buffer_desc desc;
    desc.nb_element           = rpr_uint(attributes[slot].size());
    desc.element_type         = RPR_BUFFER_ELEMENT_TYPE_FLOAT32;
    desc.element_channel_size = 4;
    CHECK(rprContextCreateBuffer(deviceState()->context, &desc, attributes[slot].data(), &m_attribute_buffers[slot]))
  }
}

const Array1D *Geometry::checkAttributeArray(const Array1D *array, size_t size, const char *level, int slot) const
{
  if (!array)
    return nullptr;
  if (array->size() < size)
  {
    reportMessage(ANARI_SEVERITY_WARNING, "array '%s.%s' has %zu elements, but %zu are required, it is ignored",
        level, attributeName(slot), array->size(), size);
    return nullptr;
  }
  return array;
}

void Geometry::setMesh(const MeshView &mesh)
{
  clearMesh();

  if (!mesh.positions || !mesh.indices || mesh.num_vertices == 0 || mesh.num_faces == 0)
    return;

  auto *state = deviceState();

  const auto start = std::chrono::steady_clock::now();

  const std::vector<rpr_int> faces(mesh.num_faces, rpr_int(mesh.face_size));

  // The first two components of 'attribute0' are also the texture coordinates of the mesh, which RPR
  // needs to calculate the tangents for normal mapping.
  const float4 *texcoords = mesh.attributes[0];

  CHECK(rprContextCreateMesh(state->context, (const rpr_float *)mesh.positions, mesh.num_vertices,
      sizeof(float3), (const rpr_float *)mesh.normals, mesh.normals ? mesh.num_vertices : 0,
      mesh.normals ? sizeof(float3) : 0, (const rpr_float *)texcoords, texcoords ? mesh.num_vertices : 0,
      texcoords ? sizeof(float4) : 0, mesh.indices, sizeof(rpr_int), mesh.normals ? mesh.indices : nullptr,
      mesh.normals ? sizeof(rpr_int) : 0, texcoords ? mesh.indices : nullptr, texcoords ? sizeof(rpr_int) : 0,
      faces.data(), mesh.num_faces, &m_base_shape))
  if (!m_base_shape)
    return;

  CHECK(rprShapeSetVisibility(m_base_shape, false)) // base shape is always invisible

  for (int slot = 0; slot < NUM_ATTRIBUTES; slot++)
  {
    if (!mesh.attributes[slot])
      continue;
    CHECK(rprShapeSetPrimvar(m_base_shape, rpr_uint(slot), (const rpr_float *)mesh.attributes[slot],
        rpr_uint(mesh.num_vertices * 4), 4, RPR_PRIMVAR_INTERPOLATION_VERTEX))
    m_has_attribute[slot] = true;
    if (m_refresh_slot < 0)
    {
      m_refresh_slot = slot;
      m_refresh_values.assign(mesh.attributes[slot], mesh.attributes[slot] + mesh.num_vertices);
    }
  }

  for (size_t i = 0; i < mesh.num_vertices; i++)
    m_bounds.extend(mesh.positions[i]);

  const auto end = std::chrono::steady_clock::now();
  reportMessage(ANARI_SEVERITY_DEBUG, "created RPR mesh with %zu vertices and %zu faces in %.2f s",
      mesh.num_vertices, mesh.num_faces, std::chrono::duration<float>(end - start).count());
}

void Geometry::instancesAttached(helium::TimeStamp build, bool primvarsNeeded) const
{
  // nothing to do for the scene in which the mesh is rendered for the first time
  if (m_first_instances_build == 0)
    m_first_instances_build = build;
  if (!primvarsNeeded || m_first_instances_build == build || m_last_refresh_build == build)
    return;
  m_last_refresh_build = build;
  if (!m_base_shape || m_refresh_slot < 0 || m_refresh_values.empty())
    return;
  CHECK(rprShapeSetPrimvar(m_base_shape, rpr_uint(m_refresh_slot), (const rpr_float *)m_refresh_values.data(),
      rpr_uint(m_refresh_values.size() * 4), 4, RPR_PRIMVAR_INTERPOLATION_VERTEX))
}

void Geometry::clearMesh()
{
  deviceState()->deleteLater(m_base_shape, DeleteOrder::SHAPE);
  m_base_shape = nullptr;
  m_has_attribute.fill(false);
  m_refresh_slot = -1;
  m_refresh_values.clear();
  m_refresh_values.shrink_to_fit();
  m_first_instances_build = 0;
  m_last_refresh_build    = 0;
  resetBounds();

  for (rpr_buffer &buffer : m_attribute_buffers)
  {
    deviceState()->deleteLater(buffer, DeleteOrder::DATA);
    buffer = nullptr;
  }
  m_instance_transforms.clear();
}

Geometry::~Geometry()
{
  clearMesh();
}

} // namespace rpr

ANARI_TYPEFOR_DEFINITION(rpr::Geometry *);

} // namespace anari
