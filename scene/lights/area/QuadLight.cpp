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

#include "QuadLight.h"

namespace anari::rpr {

QuadLight::QuadLight(RPRGlobalState *s) : Light(s) {}

void QuadLight::commitParameters()
{
  Light::commitParameters();
  m_position = getParam<float3>("position", float3(0.f));
  m_edge1    = getParam<float3>("edge1", float3(1.f, 0.f, 0.f));
  m_edge2    = getParam<float3>("edge2", float3(0.f, 1.f, 0.f));
  m_side     = getParamString("side", "front");

  const float area  = linalg::length(linalg::cross(m_edge1, m_edge2));
  const float sides = m_side == "both" ? 2.f : 1.f;
  if (hasParam("radiance") || (!hasParam("intensity") && !hasParam("power")))
  {
    m_radiance = getParam<float>("radiance", 1.f);
  } else if (hasParam("intensity"))
  {
    // intensity in the direction of the normal
    m_radiance = area > 0.f ? getParam<float>("intensity", 1.f) / area : 0.f;
  } else
  {
    // a diffuse emitter with the radiance L emits the power L * pi * area on every side
    m_radiance = area > 0.f ? getParam<float>("power", 1.f) / (pi * area * sides) : 0.f;
  }
}

void QuadLight::finalize()
{
  clear();

  auto *state = deviceState();
  CHECK(rprMaterialSystemCreateNode(state->matsys, RPR_MATERIAL_NODE_UBERV2, &m_material))
  if (!m_material)
    return;

  const float3   emission = m_color * m_radiance;
  const rpr_uint mode     = m_side == "both" ? RPR_UBER_MATERIAL_EMISSION_MODE_DOUBLESIDED
                                             : RPR_UBER_MATERIAL_EMISSION_MODE_SINGLESIDED;
  CHECK(rprMaterialNodeSetInputFByKey(
      m_material, RPR_MATERIAL_INPUT_UBER_EMISSION_COLOR, emission.x, emission.y, emission.z, 1.f))
  CHECK(rprMaterialNodeSetInputFByKey(m_material, RPR_MATERIAL_INPUT_UBER_EMISSION_WEIGHT, 1.f, 1.f, 1.f, 1.f))
  CHECK(rprMaterialNodeSetInputUByKey(m_material, RPR_MATERIAL_INPUT_UBER_EMISSION_MODE, mode))
  // the light does not reflect anything
  CHECK(rprMaterialNodeSetInputFByKey(m_material, RPR_MATERIAL_INPUT_UBER_DIFFUSE_COLOR, 0.f, 0.f, 0.f, 1.f))
  CHECK(rprMaterialNodeSetInputFByKey(m_material, RPR_MATERIAL_INPUT_UBER_DIFFUSE_WEIGHT, 0.f, 0.f, 0.f, 0.f))
  CHECK(rprMaterialNodeSetInputFByKey(m_material, RPR_MATERIAL_INPUT_UBER_REFLECTION_WEIGHT, 0.f, 0.f, 0.f, 0.f))
}

void QuadLight::attachToScene(rpr_scene scene, const mat4 &transform, SceneItems &items)
{
  if (!m_material)
    return;

  // The front side of the light is the one with the normal cross(edge1, edge2), single sided emissive
  // materials emit on the front side of the faces.
  const bool    back        = m_side == "back";
  const float3  vertices[4] = {m_position, m_position + m_edge1, m_position + m_edge1 + m_edge2, m_position + m_edge2};
  const rpr_int front[4]    = {0, 1, 2, 3};
  const rpr_int reversed[4] = {3, 2, 1, 0};
  const rpr_int faces[1]    = {4};

  // create new mesh per instance because emissive material assigned to rpr instance produces core crash
  rpr_shape shape = nullptr;
  CHECK(rprContextCreateMesh(deviceState()->context, (const rpr_float *)vertices, 4, sizeof(float3), nullptr, 0,
      0, nullptr, 0, 0, back ? reversed : front, sizeof(rpr_int), nullptr, 0, nullptr, 0, faces, 1, &shape))
  if (!shape)
    return;
  CHECK(rprShapeSetTransform(shape, false, &transform[0][0]))
  CHECK(rprShapeSetMaterial(shape, m_material))
  if (!m_visible)
  {
    // the light still illuminates the scene, but it is not seen by the camera
    CHECK(rprShapeSetVisibilityFlag(shape, RPR_SHAPE_VISIBILITY_PRIMARY_ONLY_FLAG, false))
  }
  CHECK(rprSceneAttachShape(scene, shape))
  items.shapes.push_back(shape);
}

void QuadLight::clear()
{
  deviceState()->deleteLater(m_material, DeleteOrder::MATERIAL);
  m_material = nullptr;
}

QuadLight::~QuadLight()
{
  clear();
}

} // namespace anari::rpr
