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

#include "Instance.h"

namespace anari {
namespace rpr {

Instance::Instance(RPRGlobalState *s) : SceneObject(ANARI_INSTANCE, s), m_transform_array(this) {}

void Instance::commitParameters()
{
  m_group           = getObjectParam<Group>(this, "group", ANARI_GROUP);
  m_transform       = getParam<mat4>("transform", mat4(linalg::identity));
  m_transform_array = getArray1D(this, "transform");

  for (int slot = 0; slot < NUM_ATTRIBUTES; slot++)
  {
    m_uniform_attributes[slot].reset();
    float4 value = DEFAULT_ATTRIBUTE_VALUE;
    if (getParam(attributeName(slot), ANARI_FLOAT32_VEC4, &value))
      m_uniform_attributes[slot] = value;
  }

  m_visibility.camera        = getParam<bool>("visibility.camera", true);
  m_visibility.diffuse       = getParam<bool>("visibility.diffuse", true);
  m_visibility.glossy        = getParam<bool>("visibility.glossy", true);
  m_visibility.transmission  = getParam<bool>("visibility.transmission", true);
  m_visibility.volumeScatter = getParam<bool>("visibility.volumeScatter", true);
  m_visibility.shadow        = getParam<bool>("visibility.shadow", true);
}

void Instance::finalize()
{
  if (!m_group)
    reportMessage(ANARI_SEVERITY_WARNING, "'group' is a required parameter!");

  if (m_transform_array && m_transform_array->elementType() != ANARI_FLOAT32_MAT4)
  {
    reportMessage(ANARI_SEVERITY_WARNING, "'transform' array elements are %s, but need to be %s",
        anari::toString(m_transform_array->elementType()), anari::toString(ANARI_FLOAT32_MAT4));
    m_transform_array = nullptr;
  }
}

bool Instance::isValid() const
{
  return m_group && m_group->isValid();
}

template <typename FCN_T>
void Instance::forEachTransform(FCN_T &&fcn) const
{
  if (m_transform_array)
  {
    for (auto *transform = m_transform_array->beginAs<mat4>(); transform != m_transform_array->endAs<mat4>();
         ++transform)
      fcn(*transform);
  } else
  {
    fcn(m_transform);
  }
}

box3 Instance::bounds() const
{
  box3 result = emptyBox();
  if (!isValid())
    return result;
  const box3 groupBounds = m_group->bounds();
  if (isEmpty(groupBounds))
    return result;
  forEachTransform([&](const mat4 &transform) { result.extend(xfmBox(transform, groupBounds)); });
  return result;
}

void Instance::attachToScene(
    rpr_scene scene, SceneItems &items, std::vector<EnvironmentLightInstance> &environmentLights) const
{
  // an instance which no ray sees contributes nothing to the scene, not even its lights
  if (!isValid() || !m_visibility.any())
    return;
  forEachTransform([&](const mat4 &transform) {
    m_group->content().attachToScene(scene, transform, m_uniform_attributes, m_visibility, items, environmentLights);
  });
}

} // namespace rpr

ANARI_TYPEFOR_DEFINITION(rpr::Instance *);

} // namespace anari
