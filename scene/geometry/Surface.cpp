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

#include "Surface.h"
#include "../../material/MatteMaterial.h"
#include "Geometry.h"

namespace anari {
namespace rpr {

Surface::Surface(RPRGlobalState *s) : SceneObject(ANARI_SURFACE, s) {}

void Surface::commitParameters()
{
  m_geometry = getObjectParam<Geometry>(this, "geometry", ANARI_GEOMETRY);
  m_material = getObjectParam<Material>(this, "material", ANARI_MATERIAL);
  m_visible  = getParam<bool>("visible", true);
}

void Surface::finalize()
{
  if (!m_geometry)
    reportMessage(ANARI_SEVERITY_WARNING, "'geometry' is a required attribute");

  if (!m_material || !m_material->isValid())
  {
    reportMessage(ANARI_SEVERITY_WARNING,
        "surface has no valid 'material', a default matte material is used instead");
    if (!m_default_material)
    {
      m_default_material = new MatteMaterial(deviceState());
      m_default_material->refDec(helium::RefType::PUBLIC); // never any public ref to this object
      m_default_material->commitParameters();
      m_default_material->finalize();
    }
  }
}

bool Surface::isValid() const
{
  return m_geometry && m_geometry->isValid();
}

box3 Surface::bounds() const
{
  return isValid() ? m_geometry->bounds() : emptyBox();
}

void Surface::attachToScene(rpr_scene scene, const mat4 &transform, const UniformAttributeSet &instanceAttributes,
    const RayVisibility &visibility, SceneItems &items)
{
  if (!isValid() || !m_visible)
    return;

  auto *state = deviceState();

  // Attributes of the instance override the ones of the geometry. Attributes which are not defined at all
  // have the default value.
  AttributeContext context;
  bool             perInstance = false;
  for (int slot = 0; slot < NUM_ATTRIBUTES; slot++)
  {
    if (instanceAttributes[slot].has_value())
      context.constant[slot] = instanceAttributes[slot];
    else if (m_geometry->attributeBuffer(slot))
      context.buffer[slot] = m_geometry->attributeBuffer(slot);
    else if (m_geometry->hasAttribute(slot))
      context.constant[slot].reset();
    else
      context.constant[slot] = m_geometry->uniformAttributes()[slot].value_or(DEFAULT_ATTRIBUTE_VALUE);
    perInstance |= context.buffer[slot] != nullptr;
  }

  Material            *material = m_material && m_material->isValid() ? m_material.ptr : m_default_material.ptr;
  const MaterialGraph *graph    = nullptr;
  if (material && perInstance)
  {
    // The nodes refer to the buffers of this geometry, so they are not shared with other surfaces. They
    // are deleted together with the shapes of the scene.
    items.graphs.push_back(material->createMaterial(context));
    graph = items.graphs.back().get();
  } else if (material)
  {
    graph = &material->getMaterial(context);
  }
  const rpr_material_node node       = graph ? graph->root : nullptr;
  const helium::TimeStamp materialID = graph ? graph->id : 0;

  const rpr_shape base = m_geometry->getBaseShape();
  // an instance is new or has another material than before, and the material reads primvars
  bool primvarsNeeded = false;

  auto attachInstance = [&](const mat4 &instanceTransform, rpr_uint objectID) {
    SurfaceInstance entry;
    const auto      pooled = items.pool.find(base);
    if (pooled != items.pool.end())
    {
      // an instance of the previous scene, it is attached to the scene already
      entry = pooled->second;
      items.pool.erase(pooled);
    } else
    {
      entry.base = base;
      CHECK(rprContextCreateInstance(state->context, base, &entry.instance))
      if (!entry.instance)
        return;
      CHECK(rprSceneAttachShape(scene, entry.instance))
    }
    CHECK(rprShapeSetTransform(entry.instance, false, &instanceTransform[0][0]))
    CHECK(rprShapeSetObjectID(entry.instance, perInstance ? objectID : 0))
    CHECK(rprShapeSetMaterial(entry.instance, node))
    primvarsNeeded |= entry.material != materialID && graph && graph->usesPrimvars;
    entry.material = materialID;
    if (!(entry.visibility == visibility))
    {
      setRayVisibility(state, entry.instance, visibility);
      entry.visibility = visibility;
    }
    items.instances.push_back(entry);
  };

  if (m_geometry->isInstanced())
  {
    rpr_uint objectID = 0;
    for (const mat4 &instanceTransform : m_geometry->instanceTransforms())
      attachInstance(linalg::mul(transform, instanceTransform), objectID++);
  } else
  {
    attachInstance(transform, 0);
  }

  // The base shape is invisible, but it has to be part of the scene. It stays attached if it was part of
  // the previous scene.
  if (items.base_shapes.insert(base).second && items.previous_base_shapes.count(base) == 0)
  {
    CHECK(rprSceneAttachShape(scene, base))
  }
  m_geometry->instancesAttached(items.build, primvarsNeeded);
}

} // namespace rpr

ANARI_TYPEFOR_DEFINITION(rpr::Surface *);
} // namespace anari
