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

#include "RingLight.h"

namespace anari::rpr {

// The 'radiant power' of a RPR disk light is the radiance of the disk in W/sr/m^2, the one of a RPR
// spot light is its radiant intensity in W/sr (both were measured with a diffuse plane).

RingLight::RingLight(RPRGlobalState *s) : AnalyticLight(s) {}

void RingLight::commitParameters()
{
  Light::commitParameters();
  m_position     = getParam<float3>("position", float3(0.f));
  m_direction    = getParam<float3>("direction", float3(0.f, 0.f, -1.f));
  m_openingAngle = std::clamp(getParam<float>("openingAngle", pi), 0.f, 2.f * pi);
  m_falloffAngle = std::clamp(getParam<float>("falloffAngle", 0.1f), 0.f, m_openingAngle * 0.5f);
  m_radius       = std::max(getParam<float>("radius", 0.f), 0.f);

  if (hasParam("innerRadius"))
    reportMessage(ANARI_SEVERITY_INFO, "parameter 'innerRadius' of ring light is not supported");

  if (hasParam("intensity") || (!hasParam("power") && !hasParam("radiance")))
  {
    m_intensity = getParam<float>("intensity", 1.f);
  } else if (hasParam("power"))
  {
    const float solidAngle = calculateSteradianAngle(m_openingAngle);
    m_intensity            = solidAngle > 0.f ? getParam<float>("power", 1.f) / solidAngle : 0.f;
  } else
  {
    m_intensity = getParam<float>("radiance", 1.f) * pi * m_radius * m_radius;
  }
}

void RingLight::attachToScene(rpr_scene scene, const mat4 &transform, SceneItems &items)
{
  rpr_light   light      = nullptr;
  const float outerAngle = m_openingAngle * 0.5f;
  if (m_radius > 0.f)
  {
    CHECK(rprContextCreateDiskLight(deviceState()->context, &light))
    if (!light)
      return;
    // a disk with the radiance L has the intensity L * pi * r^2 in the direction of its normal
    const float3 power = m_color * m_intensity / (pi * m_radius * m_radius);
    CHECK(rprDiskLightSetRadius(light, m_radius))
    CHECK(rprDiskLightSetAngle(light, outerAngle))
    CHECK(rprDiskLightSetInnerAngle(light, outerAngle - m_falloffAngle))
    CHECK(rprDiskLightSetRadiantPower3f(light, power.x, power.y, power.z))
  } else
  {
    // a ring without radius is a spot light
    CHECK(rprContextCreateSpotLight(deviceState()->context, &light))
    if (!light)
      return;
    const float3 power = m_color * m_intensity;
    CHECK(rprSpotLightSetRadiantPower3f(light, power.x, power.y, power.z))
    CHECK(rprSpotLightSetConeShape(light, outerAngle - m_falloffAngle, outerAngle))
  }
  attachLight(scene, light, linalg::mul(transform, rotationFromDirection(m_direction, m_position)), items);
}

} // namespace anari::rpr
