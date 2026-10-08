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

#include "HdriLight.h"

namespace anari::rpr {

HdriLight::HdriLight(RPRGlobalState *s) : AnalyticLight(s), m_radiance(this) {}

void HdriLight::commitParameters()
{
  Light::commitParameters();
  m_direction = getParam<float3>("direction", float3(1.f, 0.f, 0.f));
  m_up        = getParam<float3>("up", float3(0.f, 0.f, 1.f));
  m_scale     = getParam<float>("scale", 1.f);
  m_radiance  = getArray2D(this, "radiance");
}

void HdriLight::finalize()
{
  if (!m_radiance)
  {
    reportMessage(ANARI_SEVERITY_WARNING, "missing required parameter 'radiance' on hdri light");
  } else if (m_radiance->elementType() != ANARI_FLOAT32_VEC3 && m_radiance->elementType() != ANARI_FLOAT32_VEC4)
  {
    reportMessage(ANARI_SEVERITY_WARNING, "'radiance' of hdri light has to be an array of FLOAT32_VEC3");
  }
}

bool HdriLight::isValid() const
{
  return m_radiance
      && (m_radiance->elementType() == ANARI_FLOAT32_VEC3 || m_radiance->elementType() == ANARI_FLOAT32_VEC4)
      && m_radiance->size(0) > 0 && m_radiance->size(1) > 0;
}

void HdriLight::attachToScene(rpr_scene, const mat4 &, SceneItems &)
{
  // the environment light of a scene is created by the world
}

bool HdriLight::isEnvironmentLight() const
{
  return true;
}

size_t HdriLight::width() const
{
  return m_radiance->size(0);
}

size_t HdriLight::height() const
{
  return m_radiance->size(1);
}

float3 HdriLight::radiance(size_t x, size_t y) const
{
  const float4 value = readAttributeValue(m_radiance.get(), y * m_radiance->size(0) + x);
  return float3(value.x, value.y, value.z) * m_color * m_scale;
}

mat4 HdriLight::orientation() const
{
  // local frame of the image: x points to the center of the image, z to its upper border
  const float3 x = safeNormalize(m_direction, float3(1.f, 0.f, 0.f));
  float3       y = linalg::cross(safeNormalize(m_up, float3(0.f, 0.f, 1.f)), x);
  if (!(linalg::length(y) > 1e-6f))
  {
    float3 unused;
    makeBasis(x, y, unused);
  }
  y              = linalg::normalize(y);
  const float3 z = linalg::cross(x, y);
  return frameMatrix(x, y, z, float3(0.f));
}

} // namespace anari::rpr
